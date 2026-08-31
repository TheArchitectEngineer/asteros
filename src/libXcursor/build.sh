#!/bin/bash
# Cross-compiles real upstream libXcursor 1.2.3
# (gitlab.freedesktop.org/xorg/lib/libxcursor, tag libXcursor-1.2.3)
# against this project's vendored X11 stack, same autotools-via-
# cross-clang recipe as the other new X11 extension libs. Needs real
# libXrender and libXfixes (both already built) -- must build last of
# the four.
#
# `configure` was pre-generated the same way as libXrender's (see that
# build.sh's comment for the exact autogen.sh invocation).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"

CLANG="$ROOT/build/tools/asteros-sdk/bin/clang -std=gnu23"
PREFIX="$ROOT/build/xorg-deps-install"

export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig"

./configure --host=x86_64-apple-darwin19 --prefix="$PREFIX" \
	CC="$CLANG" \
	--disable-shared --enable-static

make -j"$(sysctl -n hw.ncpu)"
make install

echo "installed: $PREFIX/lib/libXcursor.a, $PREFIX/include/X11/Xcursor/Xcursor.h"
