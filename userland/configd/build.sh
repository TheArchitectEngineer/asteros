#!/bin/bash
# Builds configd: the real, vendored configd.tproj server logic
# (session/store/pattern/notify -- apple-oss-distributions/configd,
# tag configd-963.270.3) linked against the real generated MIG stubs
# (config.defs -> configServer.c/config.h, built by the real migcom
# vendored for Phase 25 -- see userland/toolchain/mig/build.sh) plus
# CoreFoundation + libSystem, same dependency shape as Security/
# libxpc's own builds.
#
# main.c (this project's own, not vendored -- see its own header
# comment) replaces real configd.tproj/configd.m's Objective-C main():
# no CFBundle plugin loading, no CFRunLoop-driven signal handling, no
# double-fork daemonizing -- this project's launchd (Phase 28) already
# supervises RunAtLoad daemons directly.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"

LIBSYSTEM="$ROOT/build/libSystem_obj/libSystem.B.dylib"
LIBCOREFOUNDATION="$ROOT/build/corefoundation_obj/libCoreFoundation.dylib"
CRT0="$ROOT/build/libc_obj/crt0.o"
LIBC_START="$ROOT/build/libc_obj/libc_start.o"
MIGDIR="$ROOT/build/configd_obj/mig"

if [ ! -f "$LIBSYSTEM" ]; then
	echo "error: $LIBSYSTEM not found -- build userland/libSystem first" >&2
	exit 1
fi
if [ ! -f "$LIBCOREFOUNDATION" ]; then
	echo "error: $LIBCOREFOUNDATION not found -- build userland/CoreFoundation first" >&2
	exit 1
fi
if [ ! -f "$MIGDIR/configServer.c" ]; then
	echo "error: $MIGDIR/configServer.c not found -- run mig on config.defs first (see userland/toolchain/mig/build.sh + config.defs)" >&2
	exit 1
fi

CLANG=/Applications/Xcode-beta.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang
OUT="$ROOT/build/configd_obj"
mkdir -p "$OUT"

CONFIGD_TPROJ="$ROOT/src/configd/configd.tproj"
SC_FPROJ="$ROOT/src/configd/SystemConfiguration.fproj"
SC_HEADERS="$ROOT/userland/SystemConfiguration/include"

CFLAGS=(-target x86_64-apple-macos10.15 -ffreestanding -fno-stack-protector
        -fno-builtin -nostdlibinc
        -I "$CONFIGD_TPROJ" -I "$SC_FPROJ" -I "$SC_HEADERS" -I "$MIGDIR"
        -I "$ROOT/userland/CoreFoundation/include" -I "$ROOT/userland/libdispatch/include"
        -isystem "$ROOT/userland/libc/include"
        -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -std=gnu11)

OBJS=()

compile() {
	local src="$1"
	local name
	name="$(basename "$src" .c)"
	"$CLANG" "${CFLAGS[@]}" -c "$src" -o "$OUT/$name.o"
	OBJS+=("$OUT/$name.o")
}

# real, vendored configd.tproj server-side files (Phase 25)
for f in session configd_server _SCD _configopen _configclose _configadd \
         _configget _configlist _confignotify _configremove _configset \
         _configunlock pattern _notifyadd _notifycancel _notifychanges \
         _notifyremove _notifyviafd _notifyviaport _notifyviasignal _snapshot; do
	compile "$CONFIGD_TPROJ/$f.c"
done

# real, vendored SystemConfiguration.fproj serialization helpers +
# session-object constructor + notify-cancel wrapper (shared with the
# client library, Milestone 5)
compile "$SC_FPROJ/SCD.c"
compile "$SC_FPROJ/SCDPrivate.c"
compile "$SC_FPROJ/SCDOpen.c"
compile "$SC_FPROJ/SCDNotifierCancel.c"

# this project's own main() (see header comment)
compile "$CONFIGD_TPROJ/main.c"

# real generated MIG server dispatch + client stubs (config.defs ->
# migcom). The client stubs (configopen()/notifycancel()/...) are dead
# code from configd's own perspective -- SCDOpen.c/SCDNotifierCancel.c
# are shared verbatim with the client library (Milestone 5) and
# reference them -- but the linker still needs them resolved even
# though configd's own _configopen.c/_notifycancel.c never call them
# in-process.
compile "$MIGDIR/configServer.c"
compile "$MIGDIR/configUser.c"

"$CLANG" -target x86_64-apple-macos10.15 -nostdlib -Wl,-no_pie -Wl,-bind_at_load -e _start \
	"$CRT0" "$LIBC_START" "${OBJS[@]}" "$LIBCOREFOUNDATION" "$LIBSYSTEM" -o "$OUT/configd"

echo "built: $OUT/configd"
file "$OUT/configd"
otool -l "$OUT/configd" | grep -A2 LC_LOAD_DYLIB
