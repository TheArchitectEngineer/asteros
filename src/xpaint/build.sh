#!/bin/bash
# Cross-compiles xpaint (real upstream 2.9.1.4, sourceforge.net/projects/xpaint,
# fetched via Debian's own xpaint_2.9.1.4.orig.tar.bz2 + a handful of
# Debian's own portability patches -- 1000-gcc-14.patch, gcc-15.patch,
# image_h_internal_ifdef.diff, typos.patch, applied directly to this
# tree -- against this project's vendored X11 stack
# (build/xorg-deps-install), same dyld-linked recipe as
# twm/xterm/xclock/xfm/xeyes/xedit.
#
# Upstream ships only an Imake build (Imakefile + per-subdir Imakefiles),
# same situation as xfm, so this is a hand-rolled substitute -- but the
# *file list and defines* below are lifted directly from Imakefile and
# rw/Imakefile (not guessed), following the same widget/format choices
# upstream's own ./configure would make for a "plain Xaw, JPEG yes, PNG/
# TIFF no" system:
#
#  - XAWPLAIN, not the bundled xaw3dxft/ (a whole second widget-set
#    fork with its own lex/yacc layout-language parser) -- upstream's
#    own configure script falls back to this "(plain & ugly) Xaw" mode
#    whenever Xaw3d/Xaw95/neXtaw aren't installed, linking plain
#    -lXaw -lXmu -lXt -lXext -lX11 -lm (its own "XawClientLibs"), which
#    is exactly this project's already-vendored Xaw (src/libxaw, proven
#    by xfm/xedit already). Every widget-set-conditional code path in
#    xpaint is a real, already-shipped upstream #ifdef (XAW3D/XAWPLAIN/
#    XAW95/NEXTAW), not something patched in here.
#  - HAVE_JPEG defined, linking -ljpeg against this project's own
#    src/libjpeg (already vendored for WindowMaker's JPEG wallpaper).
#  - HAVE_TIFF/HAVE_PNG left undefined (no libtiff/libpng vendored) --
#    rw/readTIFF.c+writeTIFF.c are skipped entirely (cleanly optional,
#    same as xeyes omitting XRENDER/PRESENT). PNG is *not* optional in
#    upstream's own rwTable.c -- it's the hardcoded default writer for
#    "no recognized extension", plus a few unconditional call sites in
#    print.c/rw/readWriteLXP.c/share/c_scripts/batch/batch.c -- so
#    rather than vendor a second image codec, rw/pngCompat.c (new,
#    written for this port) forwards those entry points to the
#    already-real, already-linked XPM reader/writer instead. See that
#    file's own comment.
#  - rw/readWriteXPL.c dropped: a stale duplicate of rw/readWriteLXP.c
#    (same TestLXP/ReadLXP/WriteLXP symbols, older/incomplete body --
#    diffed the two, LXP is the current one actually referenced by
#    upstream's own rw/Imakefile SRCS).
#  - rw/readWriteJP2.c dropped: unconditionally #includes <openjpeg.h>
#    (not vendored here) but is otherwise dead code -- not in upstream's
#    own rw/Imakefile SRCS, and nothing else in the tree references
#    Read/Write/TestJP2.
#  - rw/readWriteSGI.c dropped: HAVE_SGI is upstream's own
#    SGIArchitecture-only opt-in (real IRIX hardware), cleanly unused
#    off that platform.
#
# main.c/fontOp.c/fontSelect.c/magnifier.c call the real Xft API
# (XftFontOpenName/XftDrawStringUtf8/XftColorAllocValue/XftListFonts/...)
# unconditionally (not just under the widget-set-specific XAW3DXFT
# guard, which is only the custom-widget *input-method* integration) --
# this project's own src/xft-stub (real XRender-color allocation +
# real bitmap-font glyph drawing, see xft_stub.c's own file comment)
# already covers the drawing/font-open path from WindowMaker/xterm/
# xclock, but was missing XftColorAllocValue/XftColorFree/
# XftInitFtLibrary/XftListFonts/XftPatternGetString/XftFontSetDestroy/
# XftTextExtents8 -- added directly to src/xft-stub (xft_stub.c +
# Xft.h + fontconfig.h's missing FC_FOUNDRY) as this port's own real,
# not-faked implementations (XftColorAllocValue/Free are genuine
# XAllocColor/XFreeColors calls; XftListFonts reports the one real
# font this stub can actually render, the same honesty standard as
# XftFontOpenName's existing behavior, rather than parroting
# FcFontList's separately-justified honest-empty answer). Rerun
# src/xft-stub/build.sh (done as part of this build) before this
# script if you're building xpaint standalone from a stale build/.
#
# Several small host-side (not cross-compiled) code-generation steps
# upstream's own Imakefile rules perform via $(CC) -- substads.c
# (a tiny standalone template-substitution tool) turns
# app-defaults/XPaint.ad.in into XPaint.ad, then XPaint.ad.h (app
# defaults compiled in as a C string array, main.c's own fallback
# resource database) and DefaultRC.txt.h (ditto for the DefaultRC
# palette/pattern file); preproc.c turns share/messages/Messages into
# messages.h's message-ID enum. Both are pure, portable, dependency-free
# C89 with no X11/target dependency, so they're built and *run* with
# the host's own `cc`, not the cross toolchain -- their output is
# ordinary generated C text consumed by the actual cross-compiled
# build below, same as autoconf's config.status.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"

CLANG="$ROOT/build/tools/asteros-sdk/bin/clang"
DEPS="$ROOT/build/xorg-deps-install"
OUT="$ROOT/build/xpaint_obj"
mkdir -p "$OUT"

echo "=== rebuilding src/xft-stub (XftColorAllocValue/XftListFonts/etc additions) ==="
"$ROOT/src/xft-stub/build.sh"

echo "=== host codegen tools (substads, preproc) ==="
cc -o "$OUT/substads" substads.c
cc -o "$OUT/preproc" preproc.c

echo "=== generating messages.h ==="
"$OUT/preproc" > messages.h

echo "=== generating app-defaults (XPaint.ad, XPaint.ad.h, DefaultRC.txt.h) ==="
( cd app-defaults && "$OUT/substads" -appdefs \
	XPAINT_VERSION "2.9.1" \
	XPAINT_SHAREDIR "/usr/share/xpaint" \
	XPAINT_PRINT_COMMAND "lpr" \
	XPAINT_POSTSCRIPT_VIEWER "gv" \
	XPAINT_EXTERN_VIEWER "display" )
cp -f app-defaults/out/XPaint XPaint.ad
"$OUT/substads" -ad2c XPaint.ad XPaint.ad.h
"$OUT/substads" -ad2c DefaultRC DefaultRC.txt.h

echo "=== bitmaps/tools -> small_tools (TOOLFLAGS unset, matches Local.config default) ==="
ln -sfn small_tools bitmaps/tools

echo "=== xaw_incdir -> vendored plain Xaw headers (XAWPLAIN mode) ==="
ln -sfn "$DEPS/include/X11/Xaw" xaw_incdir

CFLAGS=(-std=gnu17 -O0 -g -Wall -Wno-parentheses -Wno-implicit-function-declaration
	-DXAWPLAIN -DHAVE_PARAM_H -DERRORBEEP -DFEATURE_FRACTAL -DHAVE_JPEG
	-DEDITOR="\"xterm -e neatvi\""
	-DSHAREDIR="\"/usr/share/xpaint\""
	-DXAPPLOADDIR="\"/usr/share/X11/app-defaults\""
	-DXPAINT_VERSION="\"2.9.1\""
	-I. -I "$DEPS/include")

XPSRC="chroma color colorEdit dialog fatBitsEdit fileBrowser fontSelect grab \
graphic hash help image imageComp iprocess magnifier main menu misc \
operation palette pattern print protocol readRC screenshot text texture \
typeConvert"

OPSRC="arcOp freehandOp boxOp brushOp circleOp fillOp fontOp lineOp pencilOp \
polygonOp splineOp selectOp sprayOp dynPenOp"

XPWIDSRC="Colormap Paint PaintEvent PaintRegion PaintUndo"

RWSRC="rwTable readWriteBMP readWriteICO readScriptC readWriteXBM \
readWritePNM readWriteXWD readWritePS readWriteLXP readGIF writeGIF \
readWriteXPM readJPEG writeJPEG libpnmrw pngCompat"

OBJS=()
fail=0
compile_one() {
	local src="$1" obj="$2"
	echo "-- $src --"
	if ! "$CLANG" "${CFLAGS[@]}" -c "$src" -o "$obj" 2>"$obj.err"; then
		fail=1
		echo "--- FAILED: $src ---"
		cat "$obj.err"
	fi
	OBJS+=("$obj")
}

for base in $XPSRC $OPSRC $XPWIDSRC; do
	compile_one "$base.c" "$OUT/$base.o"
done
mkdir -p "$OUT/rw"
for base in $RWSRC; do
	compile_one "rw/$base.c" "$OUT/rw/$base.o"
done

if [ "$fail" -ne 0 ]; then
	echo
	echo "=== BUILD FAILED, see errors above ==="
	exit 1
fi

# XawClientLibs-equivalent link line (upstream's own default-Xaw
# SYS_LIBRARIES from ./configure, "XawClientLibs -lm") plus -ljpeg for
# HAVE_JPEG and -lXft -lfontconfig for the Xft calls above (both
# already vendored/staged in $DEPS, see file comment).
"$CLANG" "${CFLAGS[@]}" \
	"${OBJS[@]}" \
	-L "$DEPS/lib" \
	-lXaw7 -lXmu -lXt -lXext -lX11 -lXpm -lXft -lfontconfig -ljpeg \
	-lxcb -lXau -lXdmcp -lSM -lICE -lm \
	-o "$OUT/xpaint"

echo "linked: $OUT/xpaint"
file "$OUT/xpaint"

DESTDIR="$ROOT/build/xorg-target-root"
install -d "$DESTDIR/bin"
install "$OUT/xpaint" "$DESTDIR/bin/xpaint"

install -d "$DESTDIR/usr/share/X11/app-defaults"
install -m644 app-defaults/out/XPaint "$DESTDIR/usr/share/X11/app-defaults/XPaint"

# bitmaps/tools (-> small_tools) is compile-time only -- operation.c's
# own OPBITMAPS #include "bitmaps/tools/*.xpm" lines bake the tool
# icons into the binary as C arrays (see operation.c), so only the
# runtime-loaded pieces (brushOp.c's brushboxResized() does
# sprintf("%s/bitmaps/brushbox.cfg", SHAREDIR) at runtime, same
# SHAREDIR-relative convention for bitmaps/brushes and bitmaps/elec)
# need to ship here.
install -d "$DESTDIR/usr/share/xpaint/bitmaps"
cp -R bitmaps/brushbox.cfg bitmaps/brushes bitmaps/elec \
	"$DESTDIR/usr/share/xpaint/bitmaps/"
install -d "$DESTDIR/usr/share/xpaint/help" "$DESTDIR/usr/share/xpaint/messages"
cp share/help/Help* "$DESTDIR/usr/share/xpaint/help/"
cp share/messages/Messages* "$DESTDIR/usr/share/xpaint/messages/"
install -m644 XPaintIcon.xpm "$DESTDIR/usr/share/xpaint/XPaintIcon.xpm"

echo "installed: $DESTDIR/bin/xpaint"
