#!/bin/bash
# Builds and prelinks HelloKext end to end, deterministically -- Phase 23
# (see TODO.md). Previously this whole pipeline (compile -> link ->
# kxld_link_tool -> gen_prelink_plist.py -> prelink_merge.py) was run by
# hand, one command at a time, with two values -- _PrelinkExecutableSize
# and the target vmaddr fed to kxld_link_tool -- typed in separately from
# where their real values are actually produced (prelink_merge.py computes
# its own page-aligned kext size independently and only ever prints it;
# gen_prelink_plist.py takes exec_size as a bare CLI argument with no
# cross-check against the real linked-kext file). That two-sources-of-truth
# gap is a real, live bug risk investigated while chasing Phase 23's
# unresolved "not present" page fault (TODO.md) -- this script closes it by
# deriving every address/size from the actual build artifacts at the point
# they're produced, never re-typing a value computed by an earlier step.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../../.. && pwd)"

SDKROOT="$ROOT/build/SDKs/MacOSX10.15.sdk"
KFW="$ROOT/src/xnu/BUILD/dst/System/Library/Frameworks/Kernel.framework/Versions/A"
KERNEL="$ROOT/build/kernel/kernel.development"
KXLD_LINK_TOOL="$ROOT/build/kextbuild/kxld_link_tool"
GEN_PLIST="$ROOT/userland/toolchain/kextbuild/gen_prelink_plist.py"
PRELINK_MERGE="$ROOT/userland/toolchain/kextbuild/prelink_merge.py"

OUT="$ROOT/build/kexts/HelloKext"
mkdir -p "$OUT"

for f in "$KERNEL" "$KXLD_LINK_TOOL" "$GEN_PLIST" "$PRELINK_MERGE"; do
	if [ ! -e "$f" ]; then
		echo "error: required input missing: $f" >&2
		exit 1
	fi
done

CLANG=/Applications/Xcode-beta.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang

BUNDLE_ID="com.asteros.HelloKext"
VERSION="1.0.0"
IO_CLASS="com_asteros_HelloKext"

# --- Step 1: compile ---
echo "[1/6] compiling HelloKext.cpp"
"$CLANG" -target x86_64-apple-macos10.15 -isysroot "$SDKROOT" \
	-fapple-kext -mkernel -nostdinc -x c++ -std=gnu++1z \
	-I "$KFW/Headers" -I "$KFW/PrivateHeaders" -isystem "$SDKROOT/usr/include" \
	-Wno-everything \
	-c HelloKext.cpp -o "$OUT/HelloKext.o"

# --- Step 2: link into a genuine MH_KEXT_BUNDLE (undefined KPI externs) ---
echo "[2/6] linking HelloKext.kext.bin"
"$CLANG" -target x86_64-apple-macos10.15 -fapple-kext -mkernel -nostdlib \
	-Xlinker -kext \
	-o "$OUT/HelloKext.kext.bin" "$OUT/HelloKext.o"

# --- Step 3: read the CURRENT, unmerged kernel's __PRELINK_TEXT vmaddr.
# This is the one fixed input that legitimately can't come from a later
# step -- it's where prelink_merge.py will place the kext, so kxld has to
# link against it as the target address up front. Read directly from the
# kernel image's own load commands (same struct layout prelink_merge.py's
# MachO.seg_fields() already parses), not hand-copied from a prior otool
# run.
echo "[3/6] reading target vmaddr from $KERNEL"
TARGET_VMADDR_HEX=$(python3 - "$KERNEL" <<'PYEOF'
import struct, sys
with open(sys.argv[1], "rb") as f:
    data = f.read()
(magic, cputype, cpusubtype, filetype, ncmds, sizeofcmds, flags, reserved) = \
    struct.unpack_from("<IiiIIIII", data, 0)
assert magic == 0xfeedfacf, "not a 64-bit Mach-O"
off = 32
for _ in range(ncmds):
    cmd, cmdsize = struct.unpack_from("<II", data, off)
    if cmd == 0x19:  # LC_SEGMENT_64
        segname = data[off+8:off+24].split(b"\x00", 1)[0].decode("ascii")
        if segname == "__PRELINK_TEXT":
            vmaddr, vmsize = struct.unpack_from("<QQ", data, off + 24)
            if vmsize != 0:
                sys.stderr.write("error: __PRELINK_TEXT already non-empty -- "
                    "kernel.development is already prelinked; rebuild it fresh "
                    "from build-kernel.sh first\n")
                sys.exit(1)
            print("0x%x" % vmaddr)
            sys.exit(0)
    off += cmdsize
sys.stderr.write("error: __PRELINK_TEXT segment not found\n")
sys.exit(1)
PYEOF
)
echo "    target vmaddr: $TARGET_VMADDR_HEX"

# --- Step 4: kxld-link against the real running kernel image ---
echo "[4/6] kxld_link_tool"
"$KXLD_LINK_TOOL" "$KERNEL" "$OUT/HelloKext.kext.bin" "$TARGET_VMADDR_HEX" \
	"$OUT/HelloKext.linked.bin" "$OUT/kmod_info_addr.txt"

KMOD_INFO_ADDR_HEX=$(tr -d '[:space:]' < "$OUT/kmod_info_addr.txt")
EXEC_SIZE=$(stat -f%z "$OUT/HelloKext.linked.bin")
echo "    kmod_info addr: $KMOD_INFO_ADDR_HEX"
echo "    exec size (real, measured just now): $EXEC_SIZE bytes"

# --- Step 5: generate the __PRELINK_INFO plist from those measured values
# -- never a remembered/typed constant. ---
echo "[5/6] gen_prelink_plist.py"
python3 "$GEN_PLIST" "$BUNDLE_ID" "$VERSION" "$BUNDLE_ID" "$IO_CLASS" \
	"$TARGET_VMADDR_HEX" "$EXEC_SIZE" "$KMOD_INFO_ADDR_HEX" \
	"$OUT/prelink_info.plist"

# --- Step 6: merge into the kernel image ---
echo "[6/6] prelink_merge.py"
python3 "$PRELINK_MERGE" "$KERNEL" "$OUT/HelloKext.linked.bin" \
	"$OUT/prelink_info.plist" "$OUT/kernel.prelinked"

echo "done: $OUT/kernel.prelinked"
