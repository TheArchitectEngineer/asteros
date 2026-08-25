#!/bin/bash
# Builds migcom (Apple's real MIG compiler, vendored verbatim at
# src/bootstrap_cmds/migcom.tproj/, tag bootstrap_cmds-121) as a HOST tool
# -- it runs on this Mac to generate C from .defs files at build time, the
# same shape as userland/toolchain/kextbuild/kxld_link_tool.c and
# prelink_merge.py. It never runs on the target OS.
#
# handler.c is deliberately excluded from the compile: real Apple's own
# mig.xcodeproj/project.pbxproj lists it in the project's file group but
# NOT in the actual "Sources" build phase (ground-truthed by grepping the
# vendored .pbxproj directly, not assumed) -- it's dead/superseded code
# left in the tree, and does fail to compile against the rest of this
# vintage's headers (references fields/globals -- IsCamelot, IsKernel,
# rtMaxReplySize, itDeallocate, itLongForm -- that don't exist anywhere
# else in this same source drop).
#
# parser.y/lexxer.l need bison/flex output named y.tab.c/y.tab.h, not
# bison's own default parser.tab.c/.h naming -- lexxer.l's own
# `#include "y.tab.h"` (ground-truthed by reading the file) expects the
# traditional yacc names, matching how Xcode's built-in .y/.l build rules
# invoke yacc/lex for this project.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../../.. && pwd)"

SRC="$ROOT/src/bootstrap_cmds/migcom.tproj"
OUT="$ROOT/build/mig_obj"
TOOLSBIN="$ROOT/build/tools/bin"
mkdir -p "$OUT" "$TOOLSBIN"

CLANG=/Applications/Xcode-beta.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang
SDK="$(xcrun --show-sdk-path)"
MIG_VERSION_STRING="bootstrap_cmds-121"

bison -d -o "$OUT/y.tab.c" "$SRC/parser.y"
flex -o "$OUT/lex.yy.c" "$SRC/lexxer.l"

"$CLANG" -isysroot "$SDK" -I"$SRC" -I"$OUT" \
    -DMIG_VERSION="\"$MIG_VERSION_STRING\"" -Wno-everything \
    -o "$OUT/migcom" \
    "$SRC/mig.c" "$SRC/error.c" "$SRC/global.c" "$SRC/header.c" \
    "$SRC/routine.c" "$SRC/server.c" "$SRC/statement.c" "$SRC/string.c" \
    "$SRC/type.c" "$SRC/user.c" "$SRC/utils.c" \
    "$OUT/y.tab.c" "$OUT/lex.yy.c"

cp "$OUT/migcom" "$TOOLSBIN/migcom"

# mig.sh looks for migcom at a path relative to itself
# (../libexec/migcom) by default; this project keeps every host tool
# flat in build/tools/bin (see xcrun/cc-nogroup), so the installed
# wrapper hardcodes MIGCOM instead of relying on that relative lookup.
sed -e "s#^migcomPath=.*#migcomPath=\"$TOOLSBIN/migcom\"#" \
    "$SRC/mig.sh" > "$TOOLSBIN/mig"
chmod +x "$TOOLSBIN/mig"

echo "built: $TOOLSBIN/migcom"
echo "built: $TOOLSBIN/mig"
"$TOOLSBIN/migcom" -version
