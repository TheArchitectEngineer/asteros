#!/bin/bash
# Cross-compiles real upstream xcompmgr 1.1.9
# (gitlab.freedesktop.org/xorg/app/xcompmgr, tag xcompmgr-1.1.9) -- the
# X.org project's own reference compositing manager, ported here as
# Phase 40's CPU compositor (drop shadows via real off-screen window
# redirection). Depends on libXcomposite, libXfixes, libXdamage,
# libXrender, libXext, libX11 -- all vendored under src/ -- plus the
# Composite/Damage/Fixes/Render server extensions in xorg-server
# (Composite was re-enabled for this phase; Damage/Render/Fixes were
# already on). Same autotools-via-cross-clang recipe as every libX*
# build.sh here.
#
# `configure` was pre-generated with `NOCONFIGURE=1
# ACLOCAL_PATH=$ROOT/src/xorg-util-macros LIBTOOLIZE=glibtoolize
# ./autogen.sh`, same as every libX* build.sh here.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"

CLANG="$ROOT/build/tools/asteros-sdk/bin/clang -std=gnu23"
PREFIX="$ROOT/build/xorg-deps-install"

export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig"

# Static-only build (no .dylib anywhere in xorg-deps-install), so
# pkg-config must be told --static or PKG_CHECK_MODULES only emits the
# public Libs: (-lXcomposite -lXfixes ... -lX11) and silently drops the
# transitive static chain libX11.a itself needs (-lxcb -lXau -lXdmcp),
# which then fails at link time with undefined _xcb_* symbols. Same
# fix as src/xedit/build.sh already uses for the identical reason.
./configure --host=x86_64-apple-darwin19 --prefix="$PREFIX" \
	CC="$CLANG" \
	PKG_CONFIG="pkg-config --static"

make -j"$(sysctl -n hw.ncpu)"
make install

echo "installed: $PREFIX/bin/xcompmgr"

# Same shared staging tree every other X11 port installs into --
# userland/mkrootfs.sh copies from here, not from xorg-deps-install
# (that prefix is for build-time headers/libs, not runtime binaries).
DESTDIR="$ROOT/build/xorg-target-root"
install -d "$DESTDIR/bin"
install "$PREFIX/bin/xcompmgr" "$DESTDIR/bin/xcompmgr"
echo "installed: $DESTDIR/bin/xcompmgr"
file "$DESTDIR/bin/xcompmgr"
