#!/bin/bash
# Builds restest: a normal dynamically-linked executable exercising the
# real, vendored res_mkquery/ns_initparse/ns_parserr DNS wire-format
# code entirely offline (no live NIC in this project yet -- see
# restest.c's own header comment) against libresolv.9.dylib +
# libSystem.B.dylib -- same pattern as userland/SystemConfiguration/
# test/build.sh.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../../.. && pwd)"

CLANG=/Applications/Xcode-beta.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang
OUT="$ROOT/build/libresolv_obj"
mkdir -p "$OUT"

CFLAGS=(-target x86_64-apple-macos10.15 -ffreestanding -fno-stack-protector
        -fno-builtin -nostdlibinc -I "$ROOT/userland/libresolv/shim"
        -I "$ROOT/src/libresolv"
        -isystem "$ROOT/userland/libc/include"
        -DINET6 -DBSD=199506
        -DBYTE_ORDER=1234 -DLITTLE_ENDIAN=1234 -DBIG_ENDIAN=4321 -DPDP_ENDIAN=3412
        -O1 -g -Wall -Wextra -Wno-unused-parameter)

"$CLANG" "${CFLAGS[@]}" -c restest.c -o "$OUT/restest_main.o"

CRT0="$ROOT/build/libc_obj/crt0.o"
LIBC_START="$ROOT/build/libc_obj/libc_start.o"
LIBSYSTEM="$ROOT/build/libSystem_obj/libSystem.B.dylib"
LIBRESOLV="$OUT/libresolv.9.dylib"

if [ ! -f "$LIBRESOLV" ]; then
	echo "error: $LIBRESOLV not found -- build userland/libresolv first" >&2
	exit 1
fi

"$CLANG" -target x86_64-apple-macos10.15 -nostdlib -Wl,-no_pie -Wl,-bind_at_load -e _start \
	"$CRT0" "$LIBC_START" "$OUT/restest_main.o" "$LIBRESOLV" "$LIBSYSTEM" -o "$OUT/restest"

echo "built: $OUT/restest"
file "$OUT/restest"
otool -l "$OUT/restest" | grep -A2 LC_LOAD_DYLIB
