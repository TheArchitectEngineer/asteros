#!/bin/bash
# Cross-compiles xeyes against this project's vendored X11 stack
# (build/xorg-deps-install) using the host-side cross toolchain, same
# dyld-linked recipe as twm/xterm/xclock/wmaker/xfm.
#
# Upstream (gitlab.freedesktop.org/xorg/app/xeyes) now builds with
# Meson, not autoconf -- setting up a meson+ninja cross toolchain just
# for this one small program isn't worth it (only 3 .c files), so this
# is a hand-rolled build.sh instead, same pattern as src/xfm/build.sh
# and src/neatvi/build.sh. XRENDER/PRESENT (optional upstream features
# needing libXrender/libxcb-present, neither vendored here) are left
# undefined -- both are cleanly #ifdef-guarded in Eyes.c/xeyes.c, so
# omitting them just compiles those code paths out rather than erroring.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"

CLANG="$ROOT/build/tools/asteros-sdk/bin/clang"
DEPS="$ROOT/build/xorg-deps-install"
OUT="$ROOT/build/xeyes_obj"
mkdir -p "$OUT"

CFLAGS=(-std=gnu23 -O0 -g -Wall
	-DPACKAGE_STRING="\"xeyes 1.3.1\""
	-I "$DEPS/include")

SRCS="Eyes transform xeyes"
OBJS=()
fail=0
for base in $SRCS; do
	echo "-- $base.c --"
	if ! "$CLANG" "${CFLAGS[@]}" -c "$base.c" -o "$OUT/$base.o" 2>"$OUT/$base.err"; then
		fail=1
		echo "--- FAILED: $base.c ---"
		cat "$OUT/$base.err"
	fi
	OBJS+=("$OUT/$base.o")
done

if [ "$fail" -ne 0 ]; then
	echo
	echo "=== BUILD FAILED, see errors above ==="
	exit 1
fi

"$CLANG" -std=gnu23 -O0 -g \
	"${OBJS[@]}" \
	-L "$DEPS/lib" \
	-lXi -lXext -lXmu -lXt -lX11 -lxcb -lXau -lXdmcp -lSM -lICE -lm \
	-o "$OUT/xeyes"

echo "linked: $OUT/xeyes"
file "$OUT/xeyes"

DESTDIR="$ROOT/build/xorg-target-root"
install -d "$DESTDIR/bin"
install "$OUT/xeyes" "$DESTDIR/bin/xeyes"
echo "installed: $DESTDIR/bin/xeyes"
