#!/bin/bash
# Cross-compiles libjpeg (real upstream IJG jpeg-9e, ijg.org/files/jpegsrc.v9e.tar.gz)
# against this project's cross toolchain, static-only, matching the same
# --enable-static --disable-shared convention WindowMaker's own configure
# was already built with (see src/wmaker/config.log). This is the missing
# dependency WindowMaker's configure.ac (WM_IMGFMT_CHECK_JPEG) auto-detects
# via a plain `-ljpeg` + `jpeglib.h` AC_CHECK_LIB probe -- no pkg-config
# file involved, ijg jpeg has never shipped one -- so installing
# libjpeg.a/jpeglib.h/jconfig.h/jmorecfg.h/jerror.h into the same
# build/xorg-deps-install tree WindowMaker's --x-includes/--x-libraries
# already point at is sufficient for a WindowMaker reconfigure to pick it
# up with no further wiring.
#
# Genuine autoconf/libtool project (unlike xfm/xeyes' hand-rolled
# build.sh) so this runs the same real configure+make+install recipe
# already proven for xedit/twm/xterm -- no autoreconf needed, jpeg-9e
# ships a pre-generated ./configure.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"

CLANG="$ROOT/build/tools/asteros-sdk/bin/clang"
DEPS="$ROOT/build/xorg-deps-install"
STAGE="$ROOT/build/libjpeg_destdir"

echo "=== configure ==="
CC="$CLANG -std=gnu17" \
CFLAGS="-g -O2" \
./configure --host=x86_64-apple-darwin19 --prefix=/usr \
	--enable-static --disable-shared

echo "=== make ==="
make -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

echo "=== make install (staged) ==="
rm -rf "$STAGE"
make install DESTDIR="$STAGE"

echo "=== install into xorg-deps-install ==="
install -d "$DEPS/lib" "$DEPS/include"
install -m644 "$STAGE/usr/lib/libjpeg.a" "$DEPS/lib/libjpeg.a"
install -m644 "$STAGE/usr/include/jpeglib.h" "$STAGE/usr/include/jconfig.h" \
	"$STAGE/usr/include/jmorecfg.h" "$STAGE/usr/include/jerror.h" \
	"$DEPS/include/"

echo "installed: $DEPS/lib/libjpeg.a"
file "$DEPS/lib/libjpeg.a"
