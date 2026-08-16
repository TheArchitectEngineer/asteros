#!/bin/bash
# Builds hc_prestaged: hello_crosscc.m compiled and linked entirely by
# AsterOS's own self-hosted toolchain (build/llvm-static-build/bin/
# clang-20, build/ld64_bin/ld64 -- Phase 10), run via Rosetta 2 on the
# host instead of inside QEMU. Both are real, statically-linked AsterOS
# x86_64 binaries using this project's own raw-syscall libc -- since
# that ABI is genuinely just real xnu's BSD syscall interface (this
# kernel's syscall numbers/conventions are unmodified upstream xnu, see
# TODO.md Phase 2), they happen to also run correctly on the host's own
# real xnu kernel under Rosetta translation. This is *not* a substitute
# for booting AsterOS -- it's how TODO.md Phase 27's on-target-link fix
# was actually iterated on (a QEMU boot-and-type cycle is ~60-90s;
# this is seconds) -- and it's also how that phase proved the on-target
# ld64 bug wasn't really about the *binary* ld64 produces: this exact
# recipe's output, staged into the disk image as a pre-built file by
# userland/mkrootfs.sh, runs correctly in QEMU (`HELLO_OBJC PASS`),
# while compiling+linking+running the same source in one live on-target
# shell session currently doesn't -- a real, separate, pre-existing
# fat16lite freshly-written-file bug (TODO.md Phase 9 item 3's fragment-
# ation class of issue), not anything wrong with the toolchain or the
# binary itself.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"

CLANG20="$ROOT/build/llvm-static-build/bin/clang-20"
LD64="$ROOT/build/ld64_bin/ld64"
RESOURCE_DIR="$ROOT/build/llvm-static-build/lib/clang/20"
LIBLTO="/Applications/Xcode-beta.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/libLTO.dylib"

OUT="$ROOT/build/helloobjc_ontarget_obj"
mkdir -p "$OUT"

"$CLANG20" --target=x86_64-apple-macos10.15 -fobjc-runtime=macosx \
    -resource-dir="$RESOURCE_DIR" \
    -nostdlibinc -isystem "$ROOT/userland/libc/include" \
    -c hello_crosscc.m -o "$OUT/hc.o"

"$LD64" -demangle -lto_library "$LIBLTO" \
    -dynamic -arch x86_64 -platform_version macos 10.15.0 10.15.0 \
    -mllvm -enable-linkonceodr-outlining \
    -o "$OUT/hc_prestaged" -no_pie -bind_at_load -e _start \
    "$ROOT/build/libc_obj/crt0.o" "$ROOT/build/libc_obj/libc_start.o" \
    "$OUT/hc.o" "$ROOT/build/toolchain_obj/dyld_stub_binder_ref.o" \
    "$ROOT/build/libobjc_obj/libobjc.A.dylib" "$ROOT/build/libSystem_obj/libSystem.B.dylib"

echo "built: $OUT/hc_prestaged"
file "$OUT/hc_prestaged"
