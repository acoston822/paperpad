#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

// Keeps a tapped button pressed for a few controller polls so a tap shorter than one game
// frame is not missed, and guarantees the game sees a release between two taps of the same
// button. Without that release, rapid taps merge into one long press: the game needs a fresh
// press edge for menus, dialogue and hammer swings, so most of the taps would be ignored.
class PaperPadTouchTapLatch {
public:
    // A new tap. If the button is still being held from a previous tap, release it for one
    // poll first so the next poll is a new press.
    void begin(uint16_t mask, uint8_t polls) {
        for (std::size_t bit = 0; bit < counters_.size(); ++bit) {
            const uint16_t bitMask = static_cast<uint16_t>(1u << bit);
            if ((mask & bitMask) == 0) continue;
            if (counters_[bit].load(std::memory_order_relaxed) != 0 &&
                gaps_[bit].load(std::memory_order_relaxed) == 0) {
                gaps_[bit].store(1, std::memory_order_relaxed);
            }
            uint8_t current = counters_[bit].load(std::memory_order_relaxed);
            while (current < polls &&
                   !counters_[bit].compare_exchange_weak(
                       current, polls, std::memory_order_relaxed)) {}
        }
    }

    // Kept for existing callers.
    void extend(uint16_t mask, uint8_t polls) { begin(mask, polls); }

    void clear(uint16_t mask) {
        for (std::size_t bit = 0; bit < counters_.size(); ++bit) {
            if ((mask & static_cast<uint16_t>(1u << bit)) != 0) {
                counters_[bit].store(0, std::memory_order_relaxed);
                gaps_[bit].store(0, std::memory_order_relaxed);
            }
        }
    }

    void clearAll() {
        for (auto& counter : counters_) {
            counter.store(0, std::memory_order_relaxed);
        }
        for (auto& gap : gaps_) {
            gap.store(0, std::memory_order_relaxed);
        }
    }

    // One controller poll. Returns the buttons the latch holds down. `releaseMask`, when given,
    // receives the buttons that must read as released this poll even if a finger is on them.
    uint16_t consume(uint16_t* releaseMask = nullptr) {
        uint16_t buttons = 0;
        uint16_t release = 0;
        for (std::size_t bit = 0; bit < counters_.size(); ++bit) {
            const uint16_t bitMask = static_cast<uint16_t>(1u << bit);
            uint8_t gap = gaps_[bit].load(std::memory_order_relaxed);
            if (gap != 0) {
                gaps_[bit].store(static_cast<uint8_t>(gap - 1), std::memory_order_relaxed);
                release |= bitMask;
                continue;
            }
            uint8_t current = counters_[bit].load(std::memory_order_relaxed);
            while (current != 0) {
                if (counters_[bit].compare_exchange_weak(
                        current, static_cast<uint8_t>(current - 1),
                        std::memory_order_relaxed)) {
                    buttons |= bitMask;
                    break;
                }
            }
        }
        if (releaseMask != nullptr) *releaseMask = release;
        return buttons;
    }

private:
    std::array<std::atomic<uint8_t>, 16> counters_{};
    std::array<std::atomic<uint8_t>, 16> gaps_{};
};
