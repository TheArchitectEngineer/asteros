#!/bin/bash
# Precompiles dyld_stub_binder_ref.S once on the host (see that file's
# own header comment for why this can't just be compiled on-target
# alongside the user's .m file) into a stable SDK object shipped
# alongside crt0.o/libc_start.o. Uses the host's real clang, same as
# every other userland/*/build.sh -- this is a link-time-only helper
# object, nothing about its own compilation needs to happen on-target.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"

CLANG=/Applications/Xcode-beta.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang
OUT="$ROOT/build/toolchain_obj"
mkdir -p "$OUT"

"$CLANG" -target x86_64-apple-macos10.15 -c dyld_stub_binder_ref.S -o "$OUT/dyld_stub_binder_ref.o"

echo "built: $OUT/dyld_stub_binder_ref.o"
