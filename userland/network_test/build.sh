#!/bin/bash
# Builds networktest: real AF_INET loopback TCP+UDP round trip, static
# -nostdlib -e _start, no dyld -- same recipe as
# userland/launchd/test/build.sh's echotest/launchctltest.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"

CLANG=/Applications/Xcode-beta.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang
OUT="$ROOT/build/network_test"
mkdir -p "$OUT"

CFLAGS=(-target x86_64-apple-macos10.15 -ffreestanding -fno-stack-protector
        -fno-builtin -nostdlibinc -isystem "$ROOT/userland/libc/include"
        -O1 -g -Wall -Wextra)

"$CLANG" "${CFLAGS[@]}" -c networktest.c -o "$OUT/networktest.o"

"$CLANG" -target x86_64-apple-macos10.15 -nostdlib -static -e _start \
	"$OUT/networktest.o" "$ROOT"/build/libc_obj/*.o -o "$OUT/networktest"

echo "built: $OUT/networktest"
file "$OUT/networktest"
