#!/usr/bin/env bash
# Build an unsigned, ROM-free PaperPad.app for iPhone and iPad (arm64).
# PadMint runs this with the player's own ROM, then package-unsigned-ipa.sh.
set -euo pipefail
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
source "$script_dir/lib/common.sh"

rom_path=
while (($#)); do
    case "$1" in
        --rom) (($# >= 2)) || die "--rom requires an absolute path"; rom_path=$2; shift 2 ;;
        *) die "usage: scripts/build-ios-device.sh [--rom /absolute/path/to/rom]" ;;
    esac
done

require_command cmake
require_command xcodebuild

"$script_dir/clone-sources.sh"
"$script_dir/verify-sources.sh"

if [[ -n "$rom_path" ]]; then
    "$script_dir/setup-decomp-tools.sh"
    "$script_dir/prepare-rom.sh" --rom "$rom_path"
    local_binutils="$PAPERPAD_ROOT/build-tools/mips-binutils-2.46.1/bin"
    PATH="$local_binutils:$PATH" "$script_dir/build-decomp.sh"
    "$script_dir/build-host-tools.sh"
    "$script_dir/generate-game.sh"
fi

[[ -f "$PAPERPAD_GENERATED/aot/paper_mario_recomp_out/lookup.cpp" ]] || \
    die "generated game sources are missing; rerun with --rom /absolute/path/to/your/ROM"

"$script_dir/build-rt64-host-tools.sh"

build_dir="$PAPERPAD_ROOT/build-ios-device"
cmake -S "$PAPERPAD_ROOT" -B "$build_dir" -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_SYSROOT=iphoneos \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO
cmake --build "$build_dir" --config Release --target PaperPad -- \
    -sdk iphoneos CODE_SIGNING_ALLOWED=NO

app="$build_dir/Release/PaperPad.app"
[[ -d "$app" ]] || die "PaperPad device app was not produced"
[[ ! -e "$app/baserom.z64" ]] || die "app unexpectedly contains a ROM"
note "PaperPad iPhone/iPad app ready: $app"
