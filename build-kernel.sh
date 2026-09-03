#!/bin/bash
# Builds the xnu-6153.141.1 kernel from src/ into build/kernel/kernel.development.
# Verified working end-to-end (Phase 2) from a clean BUILD/ dir on this host.
# See docs/architecture.md and patches/*.md for why each step below exists.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$ROOT/src"
SDKROOT="$ROOT/build/SDKs/MacOSX10.15.sdk"
TOOLS_BIN="$ROOT/build/tools/bin"
XCRUN_WRAPPER="$TOOLS_BIN/xcrun"

# build/ is entirely gitignored, so the wrapper has to be (re)materialized
# here on every run rather than just living on disk as a hand-created file
# -- otherwise a fresh clone, or a `make clean` that removes build/tools,
# silently loses this fix and the original "real xcrun can't resolve our
# SDK path" bug (patches/0002, patches/0017) comes back with no wrapper at
# all. Always overwritten (not "only if missing") so this script's own
# content is the one source of truth -- a stale hand-edited copy on disk
# can never drift from what's actually tracked in git.
mkdir -p "$TOOLS_BIN"
cat > "$XCRUN_WRAPPER" <<'XCRUN_WRAPPER_EOF'
#!/bin/bash
# Shim xcrun for the xnu build.
#
# xnu's makedefs/MakeInc.cmd assumes SDKROOT is a *name* xcrun understands
# (e.g. "macosx", "macosx.internal") and shells out to
# `xcrun -sdk $(SDKROOT) -show-sdk-path` / `-find <tool>` / etc.  We instead
# want SDKROOT to be our own local, header-patched copy of the SDK
# (build/SDKs/MacOSX10.15.sdk), which real xcrun refuses to resolve via
# `-sdk <path>` on this Xcode/macOS version ("SDK ... cannot be located").
#
# This wrapper intercepts just the `-sdk <OUR_SDK_PATH> ...` calls xnu's
# build makes and answers them directly; everything else (in particular
# `-find <tool>`, since the actual clang/ld/mig/etc binaries are the same
# regardless of sysroot) is forwarded to the real system xcrun using the
# real "macosx" SDK.
#
# Path matching is done by filesystem identity (device:inode via `stat`),
# not a string compare and NOT `cd ... && pwd -P` either -- both were tried
# and ground-truthed live to be unreliable here: on this case-insensitive-
# but-case-preserving APFS volume, `pwd`/$PWD inside a shell reflects
# whatever literal case was used to `cd` into the tree (e.g. a differently-
# cased Terminal tab), so the SDKROOT string a `make` invocation ends up
# with can legitimately differ in case from whatever case this script
# itself sees, even though both name the exact same on-disk directory. A
# naive `[[ "$sdk_arg" == "$OUR_SDK" ]]` string compare mismatches in that
# case (confirmed live) and falls through to the real, broken-for-our-path
# xcrun. `cd "$path" && pwd -P` was tried next on the theory that physical-
# path resolution would normalize the case, and does when run standalone
# at a shell prompt, but was confirmed live to sometimes return the
# still-differently-cased input path when run inside a script invoked as a
# fresh `bash somescript` process instead (an APFS directory-entry-cache
# quirk, not something worth depending on either way). `stat -f '%d:%i'`
# (device+inode) is the one thing ground-truthed to be genuinely identical
# for both case-spellings of the same directory every time, regardless of
# how it's invoked -- it answers "is this the same directory" directly,
# rather than trying to get two path *strings* to agree.
set -e

REAL_XCRUN=/usr/bin/xcrun

dir_id() {
    # Empty output (not an error) for a nonexistent/inaccessible path --
    # callers treat "" as "can't be our SDK, don't intercept".
    stat -f '%d:%i' "$1" 2>/dev/null || true
}

# OUR_SDK is derived from *this script's own* location, not a hardcoded
# absolute path baked in at some prior point in time -- this script lives
# at build/tools/bin/xcrun, so the SDK is two directories up from build/.
SELF_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEFAULT_SDK="$SELF_DIR/../../SDKs/MacOSX10.15.sdk"
OUR_SDK="${DARWINBUILD_SDKROOT:-$DEFAULT_SDK}"
OUR_SDK_ID="$(dir_id "$OUR_SDK")"

args=("$@")
sdk_arg=""
for i in "${!args[@]}"; do
    if [[ "${args[$i]}" == "-sdk" ]]; then
        next=$((i + 1))
        sdk_arg="${args[$next]}"
        break
    fi
done

if [ -n "$OUR_SDK_ID" ] && [ -n "$sdk_arg" ] && [ "$(dir_id "$sdk_arg")" = "$OUR_SDK_ID" ]; then
    for a in "$@"; do
        case "$a" in
            -show-sdk-path) echo "$sdk_arg"; exit 0 ;;
            -show-sdk-version) echo "10.15"; exit 0 ;;
            -show-sdk-platform-path) exec "$REAL_XCRUN" -sdk macosx -show-sdk-platform-path ;;
            -show-sdk-platform-version) echo "10.15"; exit 0 ;;
        esac
    done
    # -find <tool> and anything else: delegate to the real macosx SDK,
    # since the actual toolchain binaries don't depend on our sysroot copy.
    new_args=()
    skip_next=0
    for a in "$@"; do
        if [[ $skip_next -eq 1 ]]; then skip_next=0; continue; fi
        if [[ "$a" == "-sdk" ]]; then new_args+=("-sdk" "macosx"); skip_next=1; continue; fi
        new_args+=("$a")
    done
    exec "$REAL_XCRUN" "${new_args[@]}"
fi

exec "$REAL_XCRUN" "$@"
XCRUN_WRAPPER_EOF
chmod +x "$XCRUN_WRAPPER"

export PATH="$TOOLS_BIN:$PATH"
# A real environment variable, not just a `make XCRUN=...` command-line
# argument -- inherited by every recursive $(MAKE) invocation xnu's
# exporthdrs/installhdrs/kernel-build steps make, unconditionally, via
# plain process-environment inheritance rather than depending on GNU
# Make's command-line-variable-to-MAKEFLAGS propagation reaching every
# one of them. src/xnu/makedefs/MakeInc.cmd's XCRUN default now reads
# this (patches/0017) instead of hardcoding /usr/bin/xcrun, which cannot
# resolve our own local SDKROOT copy on this host (patches/0002).
export DARWINBUILD_XCRUN="$XCRUN_WRAPPER"

log() { printf '\n=== %s ===\n' "$1"; }

# --- Step 0: local SDK copy (only if missing) ---
if [ ! -d "$SDKROOT" ]; then
	log "Cloning local SDK copy"
	REAL_SDK=$(xcrun -sdk macosx --show-sdk-path)
	mkdir -p "$ROOT/build/SDKs"
	ditto "$REAL_SDK" "$SDKROOT"
fi

# --- Step 1: ctf tools (from dtrace) ---
if [ ! -x "$TOOLS_BIN/ctfmerge" ]; then
	log "Building ctf tools (ctfconvert, ctfdump, ctfmerge)"
	mkdir -p "$ROOT/build/dtrace/obj" "$ROOT/build/dtrace/sym" "$ROOT/build/dtrace/dst"
	(cd "$SRC/dtrace" && xcodebuild -project dtrace.xcodeproj \
		-target ctfconvert -target ctfdump -target ctfmerge \
		SDKROOT="$SDKROOT" SRCROOT="$(pwd)" \
		OBJROOT="$ROOT/build/dtrace/obj" SYMROOT="$ROOT/build/dtrace/sym" DSTROOT="$ROOT/build/dtrace/dst" \
		CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO CODE_SIGN_IDENTITY="" \
		ARCHS=x86_64 ONLY_ACTIVE_ARCH=NO build)
	mkdir -p "$TOOLS_BIN"
	cp "$ROOT/build/dtrace/sym/Release/ctfconvert" \
	   "$ROOT/build/dtrace/sym/Release/ctfdump" \
	   "$ROOT/build/dtrace/sym/Release/ctfmerge" "$TOOLS_BIN/"
fi

# --- Step 2: AvailabilityVersions ---
if [ ! -f "$SDKROOT/usr/include/AvailabilityVersions.h" ]; then
	log "Installing AvailabilityVersions headers"
	(cd "$SRC/AvailabilityVersions" && make install SRCROOT="$(pwd)" OBJROOT=obj SYMROOT=sym DSTROOT="$SDKROOT" || true)
	(cd "$SRC/AvailabilityVersions" && make install_script SRCROOT="$(pwd)" DSTROOT="$SDKROOT")
fi

# --- Step 3: libplatform headers ---
if [ ! -f "$SDKROOT/usr/local/include/os/internal/internal_shared.h" ]; then
	log "Installing libplatform headers"
	LP="$SRC/libplatform"
	mkdir -p "$SDKROOT/usr/local/include/os/internal" "$SDKROOT/usr/local/include/libkern"
	cp -f "$LP/include/os/assumes.h" "$SDKROOT/usr/include/os/assumes.h"
	cp -f "$LP/internal/os/internal.h" "$LP/internal/os/internal_asm.h" "$LP/internal/os/yield.h" \
	      "$LP/private/os/internal/internal_shared.h" "$LP/private/os/internal/atomic.h" \
	      "$LP/private/os/internal/crashlog.h" "$SDKROOT/usr/local/include/os/internal/"
	cp -f "$LP/include/libkern/"*.h "$SDKROOT/usr/local/include/libkern/"
fi

# --- Path-drift guard: stale src/xnu/BUILD/ if SRCROOT has moved ---
# xnu's recursive make embeds the absolute SRCROOT it was invoked from
# into multiple places in $SRC/xnu/BUILD/ that incremental builds then
# trust verbatim:
#   - per-component generated Makefiles (`SOURCE_DIR=/abs/path/xnu` line
#     baked in by SETUP/config/doconf -> SETUP/config/config)
#   - cached .CFLAGS / .LDFLAGS / .CXXFLAGS / .SFLAGS (REPLACECONTENTS
#     files containing the full `-I /abs/path/xnu/...` flags list)
#   - every .d / .cpd dependency file produced by clang -MD, which
#     records the absolute path of every #include it resolved during
#     the previous compile
# Moving or renaming the repo (or just `git clone`ing it fresh
# somewhere else) leaves all of those pointing at the *previous*
# absolute path. installhdrs/exporthdrs still succeed because they
# only write to BUILD/obj/EXPORT_HDRS/, not to those per-component
# generated files -- but the kernel build step then hits "No rule to
# make target `/old/abs/path/xnu/libsa/bootstrap.cpp'" because the
# stale .cpd files reference the old path as a prerequisite that no
# rule can satisfy.
#
# Detection: fingerprint $SRC/xnu as device:inode (the same trick
# patches/0017 uses for the xcrun wrapper -- immune to APFS
# case-spelling, symlinks, and `pwd -P` weirdness, just "is this the
# same on-disk directory as last time"). Compare to whatever's
# recorded under $ROOT/build/kernel/.srcroot-stamp. If the stamp
# exists but doesn't match -> repo moved, clean $SRC/xnu/BUILD/.
# Fallback for when the stamp is missing (e.g. user did
# `rm -rf build/kernel` without realising src/xnu/BUILD/ is also
# stale): grep a sample per-component generated Makefile for its
# SOURCE_DIR= line and stat *that* path; if it resolves to a
# different inode than the current $SRC/xnu, same conclusion.
SRCROOT_STAMP="$ROOT/build/kernel/.srcroot-stamp"
if [ -d "$SRC/xnu/BUILD" ]; then
	current_fp="$(cd "$SRC/xnu" && stat -f '%d:%i' . 2>/dev/null || true)"
	if [ -n "$current_fp" ]; then
		stale=0
		if [ -f "$SRCROOT_STAMP" ]; then
			[ "$(cat "$SRCROOT_STAMP" 2>/dev/null)" != "$current_fp" ] && stale=1
		else
			# No stamp -- peek at the first per-component Makefile's
			# SOURCE_DIR= and see if it still points at the same dir.
			sample_mk="$(ls "$SRC/xnu/BUILD/obj/DEVELOPMENT_X86_64"/*/DEVELOPMENT/Makefile 2>/dev/null | head -1 || true)"
			if [ -n "$sample_mk" ]; then
				old_src="$(grep -m1 '^SOURCE_DIR=' "$sample_mk" 2>/dev/null | sed 's/^SOURCE_DIR=//')"
				if [ -n "$old_src" ]; then
					if [ ! -e "$old_src" ]; then
						stale=1
					else
						old_fp="$(stat -f '%d:%i' "$old_src" 2>/dev/null || true)"
						if [ -z "$old_fp" ] || [ "$old_fp" != "$current_fp" ]; then
							stale=1
						fi
					fi
				fi
			fi
		fi
		if [ "$stale" = "1" ]; then
			log "xnu SRCROOT changed since last successful build -- removing stale $SRC/xnu/BUILD/"
			rm -rf "$SRC/xnu/BUILD"
			# HDRS_STAMP (Step 4/7 below) lives under build/kernel/, not
			# under $SRC/xnu/BUILD/, so the rm -rf above doesn't touch it --
			# left alone, Step 7 (exporthdrs, which is what actually
			# creates $SRC/xnu/BUILD/obj/EXPORT_HDRS/libsa) sees the stamp
			# as still valid and skips itself, leaving the kernel build to
			# fail immediately with "no such include directory:
			# .../EXPORT_HDRS/libsa". Removing it here keeps the two
			# staleness signals in sync: whatever invalidates BUILD/ must
			# also invalidate the header-export stamp.
			rm -f "$ROOT/build/kernel/.hdrs-stamp"
		fi
		unset stale sample_mk old_src old_fp current_fp
	fi
fi

# --- Step 4: xnu installhdrs (needed before libfirehose_kernel) ---
# Guarded by HDRS_STAMP (see Step 7 below): installhdrs/the Step 6 cp -f/
# exporthdrs all unconditionally rewrite header files every invocation,
# which was bumping their mtimes on every `make run` even when nothing
# under src/xnu actually changed -- xnu's own kernel make then saw those
# headers as "newer" and recompiled the handful of .c files that include
# them (log.c, OSKext.cpp, subr_log.c), which cascaded into a full
# relink/restrip/CTF pass and, via build-kernel.sh's own cmp-before-cp
# below, an unnecessary downstream image rebuild too. Skipping this block
# once it's already run once is safe: these are xnu's own exported
# headers, not something this project's own source changes affect.
HDRS_STAMP="$ROOT/build/kernel/.hdrs-stamp"
if [ ! -f "$HDRS_STAMP" ]; then
log "xnu installhdrs"
(cd "$SRC/xnu" && make SDKROOT="$SDKROOT" ARCH_CONFIGS=X86_64 XCRUN="$XCRUN_WRAPPER" installhdrs)

# --- Step 5: libfirehose_kernel (from libdispatch) ---
if [ ! -f "$SDKROOT/usr/local/lib/kernel/libfirehose_kernel.a" ]; then
	log "Building libfirehose_kernel"
	XNU_KFW="$SRC/xnu/BUILD/dst/System/Library/Frameworks/Kernel.framework"
	XNU_DST="$SRC/xnu/BUILD/dst"
	(cd "$SRC/libdispatch" && xcodebuild -project libdispatch.xcodeproj -target libfirehose_kernel \
		SDKROOT="$SDKROOT" SRCROOT="$(pwd)" \
		OBJROOT="$ROOT/build/libdispatch/obj" SYMROOT="$ROOT/build/libdispatch/sym" DSTROOT="$ROOT/build/libdispatch/dst" \
		CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO CODE_SIGN_IDENTITY="" \
		ARCHS=x86_64 ONLY_ACTIVE_ARCH=NO VALID_ARCHS=x86_64 \
		HEADER_SEARCH_PATHS="\$(inherited) $XNU_KFW/PrivateHeaders $XNU_KFW/Headers $XNU_DST/usr/local/include/firehose $XNU_DST/usr/local/include/os" \
		build)
	mkdir -p "$SDKROOT/usr/local/lib/kernel"
	cp "$ROOT/build/libdispatch/sym/Release/libfirehose_kernel.a" \
	   "$ROOT/build/libdispatch/sym/Release/libfirehose_kernel_debug.a" \
	   "$ROOT/build/libdispatch/sym/Release/libfirehose_kernel_profile.a" \
	   "$SDKROOT/usr/local/lib/kernel/"
fi

# --- Step 6: also mirror the firehose private headers into the kernel's own -nostdinc path ---
mkdir -p "$SDKROOT/usr/local/include/kernel/os"
cp -f "$SRC/libdispatch/os/firehose_buffer_private.h" "$SRC/libdispatch/os/firehose_server_private.h" \
      "$SRC/libplatform/private/os/base_private.h" "$SDKROOT/usr/local/include/kernel/os/"

# --- Step 7: exporthdrs (own pass, before "all" -- see patches/) ---
log "xnu exporthdrs"
mkdir -p "$ROOT/build/kernel"
mkdir -p "$SRC/xnu/BUILD/obj/EXPORT_HDRS/libsa"
(cd "$SRC/xnu" && make SDKROOT="$SDKROOT" ARCH_CONFIGS=X86_64 KERNEL_CONFIGS=DEVELOPMENT XCRUN="$XCRUN_WRAPPER" exporthdrs)
touch "$HDRS_STAMP"
else
	log "xnu installhdrs/exporthdrs already run once -- skipping (rm $HDRS_STAMP to force)"
fi

# --- Step 8: the kernel itself ---
# SLIDE=0x10 (see makedefs/MakeInc.def: KERNEL_STATIC_SLIDE = SLIDE << 21)
# moves the kernel's static link/load address from the default
# 0xffffff8000200000 (phys 0x200000, i.e. 2MiB) up to 0xffffff8002200000
# (phys 0x2200000, ~34MiB). The default 2MiB address falls inside memory
# OVMF/QEMU's own EFI memory map has already claimed by the time our
# bootloader tries to AllocatePages(AllocateAddress, ...) there, so the
# load fails outright ("AllocatePages(segment) failed", boot dies before
# even reaching ExitBootServices). This was discovered and fixed earlier
# in the project but the SLIDE value was only ever passed ad hoc on the
# command line, so it was lost once the shell session that set it ended;
# baking it into the script here makes every future rebuild reproducible.
log "Building the kernel (this is the long step)"
(cd "$SRC/xnu" && make SDKROOT="$SDKROOT" ARCH_CONFIGS=X86_64 KERNEL_CONFIGS=DEVELOPMENT XCRUN="$XCRUN_WRAPPER" SLIDE=0x10 -j"$(sysctl -n hw.ncpu)")

mkdir -p "$ROOT/build/kernel"
# cmp-before-cp: xnu's own make above is already a fast no-op when nothing
# under src/xnu changed, but an unconditional cp here would still bump
# kernel.development's mtime on every invocation regardless -- and the
# top-level Makefile's $(ESP_IMG) rule depends on that file's mtime to
# decide whether `make run` needs to redo the (expensive, always-from-
# scratch, see mkrootfs.sh's header comment) image assembly. Only touch
# the destination when the built kernel actually differs.
if ! cmp -s "$SRC/xnu/BUILD/obj/DEVELOPMENT_X86_64/kernel.development" "$ROOT/build/kernel/kernel.development" 2>/dev/null; then
	cp "$SRC/xnu/BUILD/obj/DEVELOPMENT_X86_64/kernel.development" "$ROOT/build/kernel/"
fi
log "Done: build/kernel/kernel.development"
file "$ROOT/build/kernel/kernel.development"

# Persist the current SRCROOT fingerprint so the path-drift guard above
# can spot the next repo move / rename on the *following* run. Recorded
# *after* the kernel link succeeds so a partial / failed build never
# locks in a fingerprint for a half-built BUILD/ tree.
mkdir -p "$ROOT/build/kernel"
current_fp="$(cd "$SRC/xnu" && stat -f '%d:%i' . 2>/dev/null || true)"
if [ -n "$current_fp" ]; then
	echo "$current_fp" > "$SRCROOT_STAMP"
fi
unset current_fp
