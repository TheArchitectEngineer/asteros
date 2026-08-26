#!/bin/bash
# Regenerates build/configd_obj/mig/{config.h,configUser.c,configServer.c}
# from the real, unmodified src/configd/SystemConfiguration.fproj/
# config.defs, using the real mig/migcom vendored for Phase 25 (see
# ../mig/build.sh).
#
# -novouchers: matches real Apple's own build practice for non-kernel
# Mach interfaces (ground-truthed against this project's own vendored
# src/xnu/libsyscall/xcodescripts/mach_install_mig.sh, which passes
# -novouchers on every one of its real mig invocations) -- suppresses
# the "BEGIN/END VOUCHER CODE" codegen block in config.h/configServer.c/
# configUser.c entirely, so no voucher_mach_msg_set() stub is needed on
# the libSystem side at all. Confirmed present and fully wired in this
# project's own vendored migcom (mig.c's IsVoucherCodeAllowed, gated in
# header.c/user.c) -- just never invoked with the flag until now.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../../.. && pwd)"

MIG="$ROOT/build/tools/bin/mig"
OUT="$ROOT/build/configd_obj/mig"
mkdir -p "$OUT"

export MIGCC=/Applications/Xcode-beta.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang

"$MIG" -novouchers \
    -arch x86_64 \
    -isysroot "$(xcrun --show-sdk-path)" \
    -header "$OUT/config.h" \
    -user "$OUT/configUser.c" \
    -server "$OUT/configServer.c" \
    "$ROOT/src/configd/SystemConfiguration.fproj/config.defs"

echo "generated: $OUT/config.h $OUT/configUser.c $OUT/configServer.c"
