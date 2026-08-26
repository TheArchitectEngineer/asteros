#!/bin/bash
# Builds libresolv.9.dylib: the real, vendored BIND-derived DNS resolver
# (apple-oss-distributions/libresolv, tag libresolv-68.140.2) -- Phase 30.
#
# Scope: the classic res_init/res_query/res_send/res_mkquery lookup API
# and its real ns_* support code (BSD Regents + ISC licensed --
# verified file-by-file, see TODO.md's Phase 30 entry), matching what
# most software actually calls for name resolution. Dropped:
# dns.c/dns_async.c/dns_util.c (Apple's own newer getaddrinfo-style
# convenience layer -- calls the mDNSResponder query path this project
# doesn't have, see res_query.c's own note); res_sendsigned.c (no
# copyright/license text anywhere in the file, excluded on provenance
# grounds); and, because they transitively need res_sendsigned.c's
# res_nsendsigned() plus /etc/protocols and /etc/services database
# lookups this project has no real backing store for,
# res_update.c/res_mkupdate.c/res_findzonecut.c/dst_api.c/
# dst_hmac_link.c/dst_support.c -- all in service of the narrower
# "dynamic DNS update" (nsupdate-style) feature, not basic resolution.
# Same reasoning drops ns_sign.c/ns_verify.c (TSIG message signing/
# verification, also needs dst_*.c) and res_data.c's own small
# res_sendsigned() (removed in place, see that file's own note).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"

LIBSYSTEM="$ROOT/build/libSystem_obj/libSystem.B.dylib"
if [ ! -f "$LIBSYSTEM" ]; then
	echo "error: $LIBSYSTEM not found -- build userland/libSystem first" >&2
	exit 1
fi

CLANG=/Applications/Xcode-beta.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang
OUT="$ROOT/build/libresolv_obj"
mkdir -p "$OUT"

SRC="$ROOT/src/libresolv"

CFLAGS=(-target x86_64-apple-macos10.15 -ffreestanding -fno-stack-protector
        -fno-builtin -fPIC -nostdlibinc
        -I "$ROOT/userland/libresolv/shim" -I "$SRC" -isystem "$ROOT/userland/libc/include"
        -DINET6 -DBSD=199506 -D__RES_9_COMPAT_STUB__
        -DBYTE_ORDER=1234 -DLITTLE_ENDIAN=1234 -DBIG_ENDIAN=4321 -DPDP_ENDIAN=3412
        "-DIN6ADDR_ANY_INIT={{{0}}}"
        -O1 -g -Wno-everything -std=gnu11)

OBJS=()
for f in base64 ns_date ns_name ns_netint ns_parse ns_print ns_samedomain \
         ns_ttl \
         res_comp res_debug res_init res_mkquery res_query res_send \
         res_data; do
	"$CLANG" "${CFLAGS[@]}" -c "$SRC/$f.c" -o "$OUT/$f.o"
	OBJS+=("$OUT/$f.o")
done

# This project's own (not vendored) definition of the classic legacy
# `_res` global resolv.h declares but real res_data.c never itself
# defines -- see legacy_res_compat.c's own header comment.
"$CLANG" "${CFLAGS[@]}" -c "$ROOT/userland/libresolv/legacy_res_compat.c" -o "$OUT/legacy_res_compat.o"
OBJS+=("$OUT/legacy_res_compat.o")

# -bind_at_load: same host ld64 lazy-stub crash as every other dylib in
# this tree (userland/dyld/test/build.sh) -- no real dyld_stub_binder yet.
"$CLANG" -target x86_64-apple-macos10.15 -nostdlib -dynamiclib -Wl,-bind_at_load \
	-install_name /usr/lib/libresolv.9.dylib \
	"${OBJS[@]}" "$LIBSYSTEM" -o "$OUT/libresolv.9.dylib"

echo "built: $OUT/libresolv.9.dylib"
file "$OUT/libresolv.9.dylib"
otool -l "$OUT/libresolv.9.dylib" | grep -A2 LC_LOAD_DYLIB
