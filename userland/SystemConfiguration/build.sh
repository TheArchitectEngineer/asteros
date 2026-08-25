#!/bin/bash
# Builds libSystemConfiguration.dylib: the real, vendored
# SystemConfiguration.fproj client library (Phase 25, Milestone 5) --
# SCDynamicStoreCreate/SetValue/CopyValue/RemoveValue/
# SetNotificationKeys/NotifyFileDescriptor/NotifyCancel against the real
# generated MIG client stubs (config.defs -> migcom, see
# userland/toolchain/mig/build.sh), on top of libCoreFoundation.dylib +
# libSystem.B.dylib -- same dependency shape as every other dylib in
# this tree (Security/libxpc's own builds).
#
# Several real source files here are shared verbatim with configd
# itself (SCD.c/SCDOpen.c/SCDPrivate.c/SCDNotifierCancel.c -- see
# SCDOpen.c's own header comment for why), so this build.sh compiles
# its own copies of the same real .c files rather than linking against
# configd's objects.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"

LIBSYSTEM="$ROOT/build/libSystem_obj/libSystem.B.dylib"
LIBCOREFOUNDATION="$ROOT/build/corefoundation_obj/libCoreFoundation.dylib"
MIGDIR="$ROOT/build/configd_obj/mig"

if [ ! -f "$LIBSYSTEM" ]; then
	echo "error: $LIBSYSTEM not found -- build userland/libSystem first" >&2
	exit 1
fi
if [ ! -f "$LIBCOREFOUNDATION" ]; then
	echo "error: $LIBCOREFOUNDATION not found -- build userland/CoreFoundation first" >&2
	exit 1
fi
if [ ! -f "$MIGDIR/configUser.c" ]; then
	echo "error: $MIGDIR/configUser.c not found -- run mig on config.defs first (see userland/toolchain/mig/build.sh + config.defs)" >&2
	exit 1
fi

CLANG=/Applications/Xcode-beta.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang
OUT="$ROOT/build/SystemConfiguration_obj"
mkdir -p "$OUT"

SC_FPROJ="$ROOT/src/configd/SystemConfiguration.fproj"
SC_HEADERS="$ROOT/userland/SystemConfiguration/include"

CFLAGS=(-target x86_64-apple-macos10.15 -ffreestanding -fno-stack-protector
        -fno-builtin -fPIC -nostdlibinc
        -I "$SC_FPROJ" -I "$SC_HEADERS" -I "$MIGDIR"
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

# real, vendored SystemConfiguration.fproj client files (Phase 25,
# Milestone 5)
for f in SCD SCDOpen SCDPrivate SCDNotifierCancel SCDGet SCDSet SCDRemove \
         SCDKeys SCDNotifierSetKeys SCDNotifierInformViaFD; do
	compile "$SC_FPROJ/$f.c"
done

# real generated MIG client stubs (config.defs -> migcom)
compile "$MIGDIR/configUser.c"

# -bind_at_load: same host ld64 lazy-stub crash as every other dylib in
# this tree (userland/dyld/test/build.sh) -- no real dyld_stub_binder yet.
"$CLANG" -target x86_64-apple-macos10.15 -nostdlib -dynamiclib -Wl,-bind_at_load \
	-install_name /usr/lib/libSystemConfiguration.dylib \
	"${OBJS[@]}" "$LIBCOREFOUNDATION" "$LIBSYSTEM" -o "$OUT/libSystemConfiguration.dylib"

echo "built: $OUT/libSystemConfiguration.dylib"
file "$OUT/libSystemConfiguration.dylib"
otool -l "$OUT/libSystemConfiguration.dylib" | grep -A2 LC_LOAD_DYLIB
