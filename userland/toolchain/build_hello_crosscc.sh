#!/bin/bash
# Builds helloobjc: the Phase 10 cross-compilation-toolchain regression
# test. Unlike every earlier phase's per-test build.sh (which spelled out
# clang's full CFLAGS/link line by hand -- target, ObjC runtime, crt0.o/
# libc_start.o, libobjc.A.dylib, libSystem.B.dylib, -no_pie/-bind_at_load/
# -e _start -- individually every time, see e.g. userland/libobjc/test/
# build.sh), this is the literal, unadorned command a real user would type:
# `clang hello_crosscc.m -o helloobjc`. All of that recipe now lives once,
# as real clang default arguments, in build/tools/asteros-sdk/bin/clang.cfg
# -- auto-loaded by clang's own config-file mechanism because
# build/tools/asteros-sdk/bin/clang is a hard link to the host's real
# Xcode clang sharing that directory (hard link, not a symlink: clang
# resolves symlinks before searching for a colocated .cfg, which would
# have pointed it at Xcode's own toolchain directory instead of ours).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"

bash setup_host_cross_toolchain.sh

mkdir -p "$ROOT/build/helloobjc_obj"
"$ROOT/build/tools/asteros-sdk/bin/clang" hello_crosscc.m -o "$ROOT/build/helloobjc_obj/helloobjc"

echo "built: $ROOT/build/helloobjc_obj/helloobjc"
file "$ROOT/build/helloobjc_obj/helloobjc"
otool -l "$ROOT/build/helloobjc_obj/helloobjc" | grep -A2 "LC_LOAD_DYLINKER\|LC_LOAD_DYLIB"
