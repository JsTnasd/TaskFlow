#!/usr/bin/env bash
set -euo pipefail

zig_bin="${ZIG_BIN:-zig}"

build_target() {
  local name="$1"
  local target="$2"
  local output_dir="build/$name"
  mkdir -p "$output_dir"
  "$zig_bin" rc -c65001 -fo "$output_dir/resources.res" resources/resources.rc
  "$zig_bin" c++ src/main.cpp "$output_dir/resources.res" \
    -target "$target" -std=c++20 -O2 -s -municode -static \
    -Wl,--subsystem,windows \
    -luser32 -lgdi32 -lcomdlg32 -ldwmapi -lwinmm -lmsimg32 \
    -o "$output_dir/TaskFlow.exe"
}

build_target windows-x64 x86_64-windows-gnu
build_target windows-x86 x86-windows-gnu

echo "Built build/windows-x64/TaskFlow.exe"
echo "Built build/windows-x86/TaskFlow.exe"
