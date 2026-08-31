#!/bin/bash
# Builds the honest Xft/fontconfig stub (see xft_stub.c for why this
# exists instead of a real vendor of freetype2+fontconfig+Xft) and
# installs headers + libXft.a/libfontconfig.a into the same shared
# staging prefix every other X11 dependency uses
# (build/xorg-deps-install), so WindowMaker's configure/make find it
# via the same -I/-L recipe as libX11, libXpm, etc.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"

CLANG="$ROOT/build/tools/asteros-sdk/bin/clang"
PREFIX="$ROOT/build/xorg-deps-install"
OUT="$ROOT/build/xft_stub"
mkdir -p "$OUT" "$PREFIX/include" "$PREFIX/lib"

"$CLANG" -std=gnu23 -O0 -g -Wall -Wextra \
	-I "$ROOT/src/xft-stub/include" -I "$PREFIX/include" \
	-c xft_stub.c -o "$OUT/xft_stub.o"

ar rcs "$OUT/libXft.a" "$OUT/xft_stub.o"
ranlib "$OUT/libXft.a"
cp "$OUT/libXft.a" "$OUT/libfontconfig.a"

cp -R include/X11 "$PREFIX/include/"
cp -R include/fontconfig "$PREFIX/include/"
cp "$OUT/libXft.a" "$PREFIX/lib/libXft.a"
cp "$OUT/libfontconfig.a" "$PREFIX/lib/libfontconfig.a"

mkdir -p "$PREFIX/lib/pkgconfig"
cat > "$PREFIX/lib/pkgconfig/xft.pc" <<PCEOF
prefix=$PREFIX
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: Xft
Description: Honest stub of Xft2 (see xft_stub.c) -- no real glyph rendering
Version: 2.3.1
Requires.private: x11
Cflags: -I\${includedir}
Libs: -L\${libdir} -lXft -lfontconfig
PCEOF

echo "installed: $PREFIX/lib/libXft.a, $PREFIX/lib/libfontconfig.a, $PREFIX/lib/pkgconfig/xft.pc"
