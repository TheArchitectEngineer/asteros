#!/bin/bash
# Cross-compiles real upstream libXfixes 5.0.3
# (gitlab.freedesktop.org/xorg/lib/libxfixes, tag libXfixes-5.0.3 --
# pinned to the 5.x generation to match this project's already-vendored
# fixesproto 5.0 protocol headers; the current 6.x generation needs
# newer proto headers not vendored here) against this project's
# vendored X11 stack, same autotools-via-cross-clang recipe as
# libXrender. Independent of libXrender/libXrandr/libXcursor, but
# built second since libXrandr/libXcursor both need real libXrender's
# headers already installed.
#
# This *replaces* the header-only xfixes.pc + hand-written Xfixes.h
# stub src/libXi/build.sh's manual session installed (libXi only
# needed the PointerBarrier typedef, never linked -lXfixes) with a
# real, linkable libXfixes.a -- the real `make install` below simply
# overwrites both, which is the intended outcome.
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

echo "installed: $PREFIX/lib/libXfixes.a, $PREFIX/include/X11/extensions/Xfixes.h"
