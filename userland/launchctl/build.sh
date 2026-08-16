#!/bin/bash
# Builds launchctl: a static, raw-syscall binary (same style as
# launchd/busybox -- no dyld) against launchd_control_client.c's client
# stubs (shared with userland/launchd/test/launchctltest.c) and the
# already-built libc_obj.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"

CLANG=/Applications/Xcode-beta.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang
OUT="$ROOT/build/launchctl_obj"
mkdir -p "$OUT"

CFLAGS=(-target x86_64-apple-macos10.15 -ffreestanding -fno-stack-protector
        -fno-builtin -nostdlibinc -I "$ROOT/userland/launchd"
        -isystem "$ROOT/userland/libc/include" -O1 -g -Wall -Wextra -Wno-unused-parameter)

"$CLANG" "${CFLAGS[@]}" -c launchctl.c -o "$OUT/launchctl_main.o"
"$CLANG" "${CFLAGS[@]}" -c "$ROOT/userland/launchd/launchd_control_client.c" -o "$OUT/launchd_control_client.o"

"$CLANG" -target x86_64-apple-macos10.15 -nostdlib -static -e _start \
	"$OUT/launchctl_main.o" "$OUT/launchd_control_client.o" "$ROOT/build/libc_obj"/*.o \
	-o "$OUT/launchctl"

echo "built: $OUT/launchctl"
file "$OUT/launchctl"
