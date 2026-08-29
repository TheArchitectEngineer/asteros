#!/bin/bash
# Builds xsetbg: the one userland program linked against the real,
# host-built dyld/libSystem/libobjc stack (see xsetbg.c's own header
# comment) rather than this project's usual -nostdlib freestanding
# pattern -- it also needs Xlib.h and the vendored libX11/libxcb static
# archives (build/xorg-deps-install), which every other userland/*/
# build.sh has no reason to reference. Two details matter here that
# don't for a freestanding build:
#
#  - stdio.h must come from this project's own libc headers
#    (userland/libc/include), not the host SDK's. The host SDK's
#    stdio.h names the stderr/stdout/stdin symbols the real Darwin way
#    (__stderrp etc, real objects only a real Darwin libSystem
#    exports); this project's libSystem.B.dylib exports them under its
#    own plainer names (_stderr etc) matching this project's own libc
#    headers -- so -nostdlibinc + -isystem this project's libc/include
#    is required even though a full macOS SDK is passed via -isysroot
#    for the target/ABI bits Xlib.h itself needs.
#  - crt0.o + libc_start.o must be linked in explicitly (matching every
#    other userland build.sh) so __libc_start actually runs before
#    main(): it's what sets `environ` from the real argv/envp handed
#    off the initial stack (dyld_start.S preserves that exact layout
#    across the dylinker hand-off), which XOpenDisplay(NULL)'s
#    getenv("DISPLAY") depends on. Skip these and DISPLAY silently
#    reads as unset even though the shell exported it.
#
# -bind_at_load avoids this project's dyld lazy-bind path (see
# userland/dyld/bind.c's own comment -- fixed now, but bind_at_load is
# also just what every other real-dyld-linked binary in this project
# already uses, so it stays for consistency) and -no_pie matches them
# too (ASLR-slid PIE main-executable support in this project's own
# dyld image-loader is unexercised/unproven; no reason for xsetbg to be
# the first thing to rely on it).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd .. && pwd)"

CLANG=/Applications/Xcode-beta.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang
SDK="$ROOT/build/SDKs/MacOSX10.15.sdk"
DEPS="$ROOT/build/xorg-deps-install"
OUT="$ROOT/build/xsetbg"
mkdir -p "$OUT"

"$CLANG" -target x86_64-apple-macos10.15 -isysroot "$SDK" \
    -nostdlibinc -isystem "$ROOT/userland/libc/include" -isystem "$DEPS/include" \
    -c xsetbg.c -o "$OUT/xsetbg.o"

"$CLANG" -target x86_64-apple-macos10.15 -isysroot "$SDK" \
    -Wl,-no_pie -Wl,-bind_at_load -Wl,-e,_start \
    "$ROOT/build/libc_obj/crt0.o" "$ROOT/build/libc_obj/libc_start.o" \
    "$OUT/xsetbg.o" -L "$DEPS/lib" -lX11 -lxcb -lXau -lXdmcp -lpthread \
    "$ROOT/build/libSystem_obj/libSystem.B.dylib" \
    -o "$OUT/xsetbg"

echo "built: $OUT/xsetbg"
