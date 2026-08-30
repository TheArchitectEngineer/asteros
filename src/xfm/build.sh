#!/bin/bash
# Cross-compiles xfm (X File Manager 1.4.3) against this project's vendored
# X11 stack (build/xorg-deps-install) using the host-side cross toolchain
# (build/tools/asteros-sdk/bin/clang, set up by
# userland/toolchain/setup_host_cross_toolchain.sh) -- same dyld-linked
# recipe as twm/xterm/xclock/wmaker, not the freestanding launchd/neatvi
# style (see those apps' own build notes in TODO.md for why the two styles
# differ).
#
# Unlike twm/xterm/xclock/wmaker, upstream xfm predates the X.Org autoconf
# conversion and ships only an Imake build (Imakefile) whose generated
# Makefile bakes in another Linux distro's own imake config templates --
# not reusable here. This script is a hand-rolled substitute, following
# the same plain-Makefile-less pattern already used for neatvi/xft-stub in
# this tree, but linked dynamically against this project's own real dyld/
# libSystem/libobjc/libc like the other X11 GUI ports (see clang.cfg).
#
# The exact set of source files and DEFINES below mirrors src/Imakefile +
# Imake.options' choices with every optional feature enabled (matches
# upstream's own recommended defaults): XPM, MAGIC_HEADERS, USE_3DICONS,
# USE_HISTORY, USE_TXT_FIELD, USE_POP_ACCEL, USE_USERINFO, USE_MENU,
# USE_TRANSLATIONS, USE_PERMS, USE_SCROLL, USE_CURSOR, USE_SELECTION,
# USE_CMAP, USE_VP_HACK -- except USE_LOG, which upstream itself ships
# commented out by default.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"

CLANG="$ROOT/build/tools/asteros-sdk/bin/clang"
DEPS="$ROOT/build/xorg-deps-install"
OUT="$ROOT/build/xfm_obj"
mkdir -p "$OUT"

# -O0, not -O2: this project's kernel has a documented lazy-FPU-trap hang
# triggered by clang-emitted SSE/vectorized stores in large C codebases
# (first hit building libx11, re-confirmed building WindowMaker -- see
# TODO.md Phase 36). xfm is a similarly large, never-before-run-on-this-
# kernel C codebase, so -O0 is applied preemptively rather than
# whack-a-mole optnone-ing functions after a silent boot hang.
# -std=gnu17, not gnu23 like this project's other, more-modern X11 ports
# (twm/xterm/wmaker all went through X.Org's autoconf/ANSI-C conversion;
# xfm 1.4.3 predates that and much of its Xt widget-method code --
# FocusForm.c, FileList.c, TextFileList.c, IconFileList.c, TextField.c,
# and the vendored regexp/ library -- still uses old K&R-style function
# definitions, which C23 removed outright (not just deprecated).
CFLAGS=(-std=gnu17 -O0 -g -Wall -Wno-parentheses -Wno-implicit-function-declaration
	-DXPM -DMAGIC_HEADERS -DENHANCE_BUGFIX -DUSE_NEW_WIDGETS
	-DENHANCE_3DICONS -DENHANCE_TXT_FIELD -DENHANCE_POP_ACCEL
	-DENHANCE_HISTORY -DENHANCE_USERINFO -DENHANCE_MENU
	-DENHANCE_TRANSLATIONS -DENHANCE_PERMS -DENHANCE_SCROLL
	-DENHANCE_CURSOR -DENHANCE_SELECTION -DENHANCE_CMAP -DVIEWPORT_HACK
	-I "$DEPS/include" -I regexp)

echo "=== regexp library ==="
mkdir -p "$OUT/regexp"
REGEXP_OBJS=()
# This vendored regexp library (Henry Spencer's, via 4.4BSD) predates
# ANSI C and uses old K&R-style function definitions
# (`regcomp(exp) char *exp; { ... }`) -- valid through C17 but removed
# in C23, so it needs its own -std, overriding the gnu23 the rest of
# this project's X11 ports use.
for base in regexp regsub regerror; do
	echo "-- regexp/$base.c --"
	"$CLANG" "${CFLAGS[@]}" -std=gnu17 -c "regexp/$base.c" -o "$OUT/regexp/$base.o"
	REGEXP_OBJS+=("$OUT/regexp/$base.o")
done
ar rcs "$OUT/libregexp.a" "${REGEXP_OBJS[@]}"
ranlib "$OUT/libregexp.a"

# SRCS1 from src/Imakefile, with every Imake.options feature above enabled
# (HISTORYSRC=FmHistory.c, TXTFIELDSRC=TextField.c+FocusForm.c,
# SELECTIONSRC=FmSelection.c, LOGSRC empty since USE_LOG is off upstream,
# MAGICSRC=magic.c since MAGIC_HEADERS is on).
XFM_SRCS="FmMain FmPopup FmUtils FmDirs FmBitmaps FmFw FmFwCb FmAw FmAwCb \
FmAwActions FmAwPopup FmFwActions FmChmod FmInfo FmErrors FmDelete \
FmConfirm FmExec FmComms FmOps XtHelper magic FmHistory FmSelection \
TextField FocusForm FmStringDefs FileList TextFileList IconFileList"

echo "=== xfm ==="
XFM_OBJS=()
fail=0
for base in $XFM_SRCS; do
	f="src/$base.c"
	echo "-- $f --"
	if ! "$CLANG" "${CFLAGS[@]}" -c "$f" -o "$OUT/$base.o" 2>"$OUT/$base.err"; then
		fail=1
		echo "--- FAILED: $f ---"
		cat "$OUT/$base.err"
	fi
	XFM_OBJS+=("$OUT/$base.o")
done

if [ "$fail" -ne 0 ]; then
	echo
	echo "=== BUILD FAILED, see errors above ==="
	exit 1
fi

# XawClientLibs-equivalent link line: real, non-obvious transitive static
# deps twm's own pkg-config-derived TWM_LIBS already established for this
# vendored X11 stack (Phase 34/35/36) -- libX11.a alone doesn't pull in
# libxcb/libXau/libXdmcp on this static link, so they're spelled out
# explicitly, same as WindowMaker's LIBS= (see TODO.md Phase 36).
"$CLANG" -std=gnu23 -O0 -g \
	"${XFM_OBJS[@]}" "$OUT/libregexp.a" \
	-L "$DEPS/lib" \
	-lXaw7 -lXmu -lXt -lXext -lX11 -lXpm -lxcb -lXau -lXdmcp -lSM -lICE -lm \
	-o "$OUT/xfm"

echo "linked: $OUT/xfm"
file "$OUT/xfm"

echo "=== xfmtype ==="
"$CLANG" "${CFLAGS[@]}" -c src/xfmtype.c -o "$OUT/xfmtype.o"
"$CLANG" -std=gnu17 -O0 -g "$OUT/xfmtype.o" "$OUT/magic.o" "$OUT/libregexp.a" -o "$OUT/xfmtype"
echo "linked: $OUT/xfmtype"
file "$OUT/xfmtype"

# --- install into build/xorg-target-root -----------------------------
# Same DESTDIR-style staging tree every other X11 port
# (twm/xterm/xclock/wmaker) installs into -- userland/mkrootfs.sh copies
# binaries and resource dirs from here, not from this build tree.
DESTDIR="$ROOT/build/xorg-target-root"
XFMLIBDIR="/usr/share/xfm"

echo "=== install ==="
install -d "$DESTDIR/bin" "$DESTDIR$XFMLIBDIR" "$DESTDIR/usr/share/X11/app-defaults"
install "$OUT/xfm" "$DESTDIR/bin/xfm"
install "$OUT/xfmtype" "$DESTDIR/bin/xfmtype"

# lib/bitmaps + lib/pixmaps: installed unconditionally by upstream's top
# Imakefile regardless of USE_3DICONS (Xfm.ad's bitmapPath always
# searches LIBDIR/bitmaps; pixmapPath falls back to LIBDIR/pixmaps after
# LIBDIR/icons).
install -d "$DESTDIR$XFMLIBDIR/bitmaps" "$DESTDIR$XFMLIBDIR/pixmaps" "$DESTDIR$XFMLIBDIR/icons"
cp lib/bitmaps/* "$DESTDIR$XFMLIBDIR/bitmaps/"
cp lib/pixmaps/* "$DESTDIR$XFMLIBDIR/pixmaps/"
# USE_3DICONS: the icon set actually enabled above (Imake.options'
# recommended default) -- contrib/3dicons/icons, not lib/pixmaps alone.
cp contrib/3dicons/icons/* "$DESTDIR$XFMLIBDIR/icons/"

# dot.xfm: the default ~/.xfm this project's rootfs pre-populates
# directly (see userland/mkrootfs.sh) instead of shipping xfm.install
# and requiring an interactive first-run `read` prompt this headless
# GUI-only OS has no good way to drive.
install -d "$DESTDIR$XFMLIBDIR/dot.xfm"
cp contrib/3dicons/xfmrc "$DESTDIR$XFMLIBDIR/dot.xfm/xfmrc"
cp contrib/3dicons/xfmdev "$DESTDIR$XFMLIBDIR/dot.xfm/xfmdev"
cp contrib/3dicons/magic "$DESTDIR$XFMLIBDIR/dot.xfm/magic"
cp contrib/3dicons/Apps "$DESTDIR$XFMLIBDIR/dot.xfm/Apps"
cp contrib/3dicons/Graphics "$DESTDIR$XFMLIBDIR/dot.xfm/Graphics"
cp contrib/3dicons/Toolbox "$DESTDIR$XFMLIBDIR/dot.xfm/Toolbox"

# App-defaults (Xfm.ad -> .../app-defaults/Xfm): normally produced by
# Imake's CppFileTarget running lib/Xfm.cpp through cpp -- but real cpp
# (clang -E, GNU cpp -traditional-cpp both tried) mis-splices this
# file's literal backslash-escaped resource values (e.g. the
# selectionPathsSeparator line) as C line-continuations, corrupting the
# #ifdef nesting. gen_appdefaults.py is a small line-oriented
# (non-C-tokenizing) stand-in that only handles what this one file
# actually needs: #ifdef/#ifndef/#else/#endif, plus LIBDIR/XFMVERSION/
# LOG_TRANSLATION/HIST_TRANSLATION substitution. Its DEFINES set must be
# kept in sync with the CFLAGS -D list above.
python3 gen_appdefaults.py lib/Xfm.cpp > "$DESTDIR/usr/share/X11/app-defaults/Xfm"

echo "installed under $DESTDIR"
