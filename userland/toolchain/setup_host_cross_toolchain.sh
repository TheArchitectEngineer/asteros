#!/bin/bash
# Regenerates build/tools/asteros-sdk/bin/{clang,clang.cfg} from the
# tracked template (host_cross_clang.cfg.in) -- build/ is always
# regenerable, never hand-edited (same discipline as every other build.sh
# in this tree). "clang" is a hard link to the host's real Xcode clang,
# not a symlink: clang resolves symlinks before searching for a colocated
# config file, which would point it at Xcode's own toolchain directory
# instead of this one, so a hard link (same volume, same inode, but its
# own directory entry) is what makes clang's config-file auto-discovery
# see build/tools/asteros-sdk/bin/clang.cfg by default.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"

CLANG_REAL="/Applications/Xcode-beta.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang"
XCODE_RESOURCE_DIR="$("$CLANG_REAL" -print-resource-dir)"
XCODE_LIBLTO="$(dirname "$(dirname "$CLANG_REAL")")/lib/libLTO.dylib"

SDK_BIN="$ROOT/build/tools/asteros-sdk/bin"
mkdir -p "$SDK_BIN"

rm -f "$SDK_BIN/clang"
ln "$CLANG_REAL" "$SDK_BIN/clang"

sed -e "s#@ROOT@#$ROOT#g" \
    -e "s#@XCODE_RESOURCE_DIR@#$XCODE_RESOURCE_DIR#g" \
    -e "s#@XCODE_LIBLTO@#$XCODE_LIBLTO#g" \
    host_cross_clang.cfg.in > "$SDK_BIN/clang.cfg"

echo "host cross toolchain ready: $SDK_BIN/clang (clang hello.m -o hello)"
