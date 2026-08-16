#!/bin/sh
# On-target counterpart to userland/toolchain/build_hello_crosscc.sh:
# compiles + links + runs hello_crosscc.m using AsterOS's own self-hosted
# clang/ld (build/llvm-static-build, build/ld64_bin -- Phase 10), staged
# at /usr/bin/clang + /usr/bin/ld, entirely on-target. --no-default-config
# skips /usr/bin/clang.cfg (set up for *static* hello-worlds, see that
# file's own header comment) -- incompatible with dynamically linking
# libobjc, so every flag is passed explicitly instead, same recipe as
# userland/toolchain/build-hello.sh's Foundation build, minus Foundation/
# CoreFoundation since hello_crosscc.m only needs libobjc + libSystem.
# Deployed as /tmp/b.sh (short name -- this gets typed at the AsterOS
# shell prompt via QEMU-monitor sendkey during verification, see
# TODO.md Phase 27's on-target follow-up).
set -e

clang --no-default-config --target=x86_64-apple-macos10.15 \
	-fobjc-runtime=macosx \
	-nostdlibinc -isystem /usr/include \
	-c /tmp/hello_crosscc.m -o /tmp/hc.o

# /usr/lib/dyld_stub_binder_ref.o: see its own source comment
# (userland/toolchain/dyld_stub_binder_ref.S) -- works around a bug in
# this on-target ld64's dyld_stub_binder auto-discovery. Prebuilt on
# the host and shipped as a stable SDK object, not compiled here.
clang --no-default-config --target=x86_64-apple-macos10.15 \
	-nostdlib -fuse-ld=/usr/bin/ld \
	-Wl,-no_pie -Wl,-bind_at_load -e _start \
	/usr/lib/crt0.o /usr/lib/libc_start.o /tmp/hc.o \
	/usr/lib/dyld_stub_binder_ref.o \
	/usr/lib/libobjc.A.dylib /usr/lib/libSystem.B.dylib \
	-o /tmp/hc

echo "ONTARGET_BUILD_OK"
/tmp/hc
