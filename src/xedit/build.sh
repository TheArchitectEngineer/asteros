#!/bin/bash
# Cross-compiles xedit (real upstream 1.2.5, gitlab.freedesktop.org/xorg/app/xedit
# -- the X.Org Athena-widget text editor with a built-in Lisp extension
# language) against this project's vendored X11 stack
# (build/xorg-deps-install) using the host-side cross toolchain.
#
# Unlike xfm/xeyes (upstream moved off autotools, needed a hand-rolled
# build.sh), xedit still ships a real configure.ac with plain
# PKG_CHECK_MODULES(xaw7 xmu xt x11) -- all already vendored here -- so
# this runs the same real in-tree autoreconf+configure+make recipe
# already proven for twm/xterm/xclock (see those apps' own committed
# config.log for the exact recipe this mirrors): --host=x86_64-apple-darwin19
# + PKG_CONFIG_LIBDIR (not _PATH, see TODO.md Phase 34's own lesson about
# pkg-config's default paths being additive) pointed at
# build/xorg-deps-install, plus ACLOCAL_PATH for the vendored xorg-macros.m4
# autoreconf needs to satisfy configure.ac's XORG_MACROS_VERSION check.
#
# -O0, not twm/xclock's -O2: xedit bundles a real bignum-math (lisp/mp,
# ported from LDB's libmp) and bytecode-interpreter (lisp/bytecode.c) Lisp
# engine -- substantially more, and more numerically involved,
# never-before-run-on-this-kernel C than twm/xclock's simple event loops.
# Treated with the same up-front -O0 caution as WindowMaker (TODO.md
# Phase 36, the documented lazy-FPU-trap/SSE-store kernel hang) rather
# than risk it and whack-a-mole optnone-ing functions after a hang.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"

CLANG="$ROOT/build/tools/asteros-sdk/bin/clang"
DEPS="$ROOT/build/xorg-deps-install"
DESTDIR="$ROOT/build/xorg-target-root"
STAGE="$ROOT/build/xedit_destdir"

export ACLOCAL_PATH="$DEPS/share/aclocal"

echo "=== autoreconf ==="
autoreconf -fi

echo "=== configure ==="
PKG_CONFIG="pkg-config --static" \
PKG_CONFIG_LIBDIR="$DEPS/lib/pkgconfig:$DEPS/share/pkgconfig" \
CC="$CLANG -std=gnu23" \
CFLAGS="-g -O0" \
LDFLAGS="-L$DEPS/lib" \
./configure --host=x86_64-apple-darwin19 --prefix=/usr --bindir=/bin \
	--datadir=/usr/share --with-appdefaultdir=/usr/share/X11/app-defaults

echo "=== make ==="
make -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

echo "=== make install (staged) ==="
rm -rf "$STAGE"
make install DESTDIR="$STAGE"

echo "=== install into xorg-target-root ==="
# Same shared staging tree every other X11 port installs into --
# userland/mkrootfs.sh copies from here, not from any app's own build tree.
install -d "$DESTDIR/bin"
install "$STAGE/bin/xedit" "$DESTDIR/bin/xedit"

install -d "$DESTDIR/usr/share/X11/app-defaults"
cp "$STAGE/usr/share/X11/app-defaults/Xedit" "$STAGE/usr/share/X11/app-defaults/Xedit-color" \
	"$DESTDIR/usr/share/X11/app-defaults/"

# LISPDIR (configure's default $libdir/X11/xedit/lisp = /usr/lib/X11/xedit/lisp)
# -- xedit's own lisp/require.c looks up modules under this path at
# runtime (autoloaded on first use of a given editing mode), so it has to
# ship even for a minimal "it starts and edits a file" install.
install -d "$DESTDIR/usr/lib/X11/xedit/lisp/progmodes"
cp "$STAGE/usr/lib/X11/xedit/lisp/"*.lsp "$DESTDIR/usr/lib/X11/xedit/lisp/"
cp "$STAGE/usr/lib/X11/xedit/lisp/progmodes/"*.lsp "$DESTDIR/usr/lib/X11/xedit/lisp/progmodes/"

echo "installed: $DESTDIR/bin/xedit"
file "$DESTDIR/bin/xedit"
