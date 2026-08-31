#!/bin/bash
# Assembles boot/fat16.img, the FAT16 image bsd/miscfs/fat16lite mounts as
# root. Reformatted from scratch every time rather than mcopy -o'd onto an
# existing image -- repeated in-place overwrites fragment the FAT cluster
# chain, which fat16lite's pager_map_to_phys_contiguous can't handle (see
# TODO.md Phase 9 item 3).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

ROOTFS_IMG="boot/fat16.img"
ROOTFS_SIZE_MB=220

CLANG_BIN="build/llvm-static-build/bin/clang"
LD_BIN="build/ld64_bin/ld64"
LIBCXX="build/runtimes-install/lib/libc++.a"
LIBCXXABI="build/runtimes-install/lib/libc++abi.a"
LIBUNWIND="build/runtimes-install/lib/libunwind.a"
CLANGRT="build/compiler-rt-install/lib/darwin/libclang_rt.osx.a"
CLANG_RESOURCE_INCLUDE="build/llvm-static-build/lib/clang/20/include"

rm -f "$ROOTFS_IMG"
dd if=/dev/zero of="$ROOTFS_IMG" bs=1m count="$ROOTFS_SIZE_MB" status=none
# Geometry (8 sectors/cluster, 8 reserved sectors, 32-sector root dir) matches
# the hand-built image fat16lite was originally verified against -- letting
# mformat pick its own defaults here produced a layout where a file's data
# start wasn't page-aligned, which pager_map_to_phys_contiguous requires
# (panics with "computed address ... is not page-aligned" otherwise).
mformat -i "$ROOTFS_IMG" -R 8 -c 8 -r 32 -h 16 -n 63 -v ROOTFS ::

for d in bin sbin dev etc tmp usr var; do
	mmd -i "$ROOTFS_IMG" "::/$d"
done
mmd -i "$ROOTFS_IMG" ::/usr/lib
mmd -i "$ROOTFS_IMG" ::/var/log

# fbdevfs (bsd/miscfs/fbdevfs/) is mounted here by bsd_init.c right after
# devfs -- must already exist on the real root filesystem, kernel_mount()
# looks it up by path (see bsd_init.c's FBDEVFS block).
mmd -i "$ROOTFS_IMG" ::/fbdev

# xnu's vm_swap_create_file() hardcodes SWAP_FILE_NAME as
# "/private/var/vm/swapfile" (osfmk/vm/vm_compressor_backing_store.h) --
# without this directory the compressor's swapfile-create thread just
# fails forever (harmlessly, but noisily: "vm_swap_create_file failed").
# Created directly at this literal path rather than as a /var symlink
# target -- fat16lite has no symlink support, and nothing else here reads
# swap files via /var/vm, so there's no need for the two paths to alias.
mmd -i "$ROOTFS_IMG" ::/private
mmd -i "$ROOTFS_IMG" ::/private/var
mmd -i "$ROOTFS_IMG" ::/private/var/vm

mcopy -i "$ROOTFS_IMG" src/busybox/busybox_unstripped ::/bin/busybox
# busybox dispatches its applets by inspecting argv[0], not by file
# identity -- real installs symlink /bin/sh -> busybox, but FAT16 has
# no symlinks/hardlinks, so this is a second physical copy under the
# name "sh". Needed for any real fork+execl("/bin/sh", "sh", "-c", ...)
# (as opposed to typing "sh" at an already-running ash prompt, which
# busybox's own shell resolves internally without ever calling exec) --
# found live via xorg-server's Popen()/System(), which xkbcomp
# invocation depends on (X11 milestone).
mcopy -i "$ROOTFS_IMG" src/busybox/busybox_unstripped ::/bin/sh
mcopy -i "$ROOTFS_IMG" src/busybox/busybox_unstripped ::/bin/sleep
mcopy -i "$ROOTFS_IMG" build/launchd/launchd ::/sbin/launchd
mcopy -i "$ROOTFS_IMG" build/launchctl_obj/launchctl ::/bin/launchctl

mmd -i "$ROOTFS_IMG" ::/etc/launchd
mmd -i "$ROOTFS_IMG" ::/etc/launchd/daemons
mcopy -i "$ROOTFS_IMG" userland/launchd/daemons/com.asteros.shell.plist ::/etc/launchd/daemons/com.asteros.shell.plist
mcopy -i "$ROOTFS_IMG" userland/launchd/daemons/com.asteros.echotest.plist ::/etc/launchd/daemons/com.asteros.echotest.plist
if [ -f build/libSystem_obj/pthreadtest ]; then
	mcopy -i "$ROOTFS_IMG" userland/launchd/daemons/com.asteros.pthreadtest.plist ::/etc/launchd/daemons/com.asteros.pthreadtest.plist
fi
if [ -f build/launchd_test/echotest ]; then
	echo "echotest found in build/ -- including it in the rootfs"
	mcopy -i "$ROOTFS_IMG" build/launchd_test/echotest ::/bin/echotest
fi
if [ -f build/launchd_test/launchctltest ]; then
	echo "launchctltest found in build/ -- including it in the rootfs"
	mcopy -i "$ROOTFS_IMG" build/launchd_test/launchctltest ::/bin/launchctltest
	mcopy -i "$ROOTFS_IMG" userland/launchd/daemons/com.asteros.launchctltest.plist ::/etc/launchd/daemons/com.asteros.launchctltest.plist
fi
if [ -f build/corefoundation_obj/cftest ]; then
	mcopy -i "$ROOTFS_IMG" userland/launchd/daemons/com.asteros.cftest.plist ::/etc/launchd/daemons/com.asteros.cftest.plist
fi
if [ -f build/Foundation_obj/foundationtest ]; then
	mcopy -i "$ROOTFS_IMG" userland/launchd/daemons/com.asteros.foundationtest.plist ::/etc/launchd/daemons/com.asteros.foundationtest.plist
fi
if [ -f build/dispatch_obj/dispatchtest ]; then
	mcopy -i "$ROOTFS_IMG" userland/launchd/daemons/com.asteros.dispatchtest.plist ::/etc/launchd/daemons/com.asteros.dispatchtest.plist
fi
if [ -f build/security_obj/securitytest ]; then
	mcopy -i "$ROOTFS_IMG" userland/launchd/daemons/com.asteros.securitytest.plist ::/etc/launchd/daemons/com.asteros.securitytest.plist
fi
if [ -f build/xpc_obj/xpctest ]; then
	mcopy -i "$ROOTFS_IMG" userland/launchd/daemons/com.asteros.xpctest.plist ::/etc/launchd/daemons/com.asteros.xpctest.plist
fi
if [ -f build/libSystem_obj/machtest ]; then
	mcopy -i "$ROOTFS_IMG" userland/launchd/daemons/com.asteros.machtest.plist ::/etc/launchd/daemons/com.asteros.machtest.plist
fi
if [ -f build/network_test/networktest ]; then
	echo "networktest found in build/ -- including it in the rootfs"
	mcopy -i "$ROOTFS_IMG" build/network_test/networktest ::/bin/networktest
	mcopy -i "$ROOTFS_IMG" userland/launchd/daemons/com.asteros.networktest.plist ::/etc/launchd/daemons/com.asteros.networktest.plist
fi
if [ -f build/fbtest/fbtest ]; then
	echo "fbtest found in build/ -- including it in the rootfs"
	mcopy -i "$ROOTFS_IMG" build/fbtest/fbtest ::/bin/fbtest
	mcopy -i "$ROOTFS_IMG" userland/launchd/daemons/com.asteros.fbtest.plist ::/etc/launchd/daemons/com.asteros.fbtest.plist
fi
if [ -f build/pstest/pstest ]; then
	echo "pstest found in build/ -- including it in the rootfs (binary only, not auto-run)"
	mcopy -i "$ROOTFS_IMG" build/pstest/pstest ::/bin/pstest
	# Not registering com.asteros.pstest.plist as a boot-time daemon:
	# /dev/psevent (bsd/dev/i386/psevent.c) is a single shared ring
	# buffer, not per-opener -- whichever reader calls read() first
	# drains an event, there's no fan-out to multiple concurrent
	# readers. pstest opens it at every boot (RunAtLoad) and actively
	# drains it for up to PSTEST_BUDGET_MS (60s) waiting to see a real
	# keystroke, which is exactly the window startx/twm/xterm come up
	# in -- it was winning the race for every real keypress before
	# Xfbdev's asterosInputRead() ever saw one, so X11 keyboard input
	# looked completely dead while the kernel console (a separate,
	# earlier delivery path in ps2_kbd.c) kept working fine. Its job
	# (proving psevent delivery works at all) is done; the binary
	# stays available at /bin/pstest for manual re-verification, it
	# just no longer runs unattended at every boot.
fi
if [ -f build/unixtest/unixtest ]; then
	echo "unixtest found in build/ -- including it in the rootfs"
	mcopy -i "$ROOTFS_IMG" build/unixtest/unixtest ::/bin/unixtest
	mcopy -i "$ROOTFS_IMG" userland/launchd/daemons/com.asteros.unixtest.plist ::/etc/launchd/daemons/com.asteros.unixtest.plist
fi
if [ -f build/xorg-target-root/bin/Xfbdev ]; then
	echo "Xfbdev found in build/ -- including it in the rootfs"
	mcopy -i "$ROOTFS_IMG" build/xorg-target-root/bin/Xfbdev ::/bin/Xfbdev
	mcopy -i "$ROOTFS_IMG" userland/startx.sh ::/bin/startx
	if [ -f build/xorg-target-root/bin/xkbcomp ]; then
		echo "xkbcomp found in build/ -- including it in the rootfs"
		mcopy -i "$ROOTFS_IMG" build/xorg-target-root/bin/xkbcomp ::/bin/xkbcomp
	fi
	if [ -d build/xorg-target-root/usr/share/X11/xkb ]; then
		echo "xkeyboard-config data found in build/ -- including it in the rootfs"
		mmd -i "$ROOTFS_IMG" ::/usr/share 2>/dev/null
		mmd -i "$ROOTFS_IMG" ::/usr/share/X11 2>/dev/null
		mcopy -s -i "$ROOTFS_IMG" build/xorg-target-root/usr/share/X11/xkb ::/usr/share/X11/
	fi
	if [ -f build/xorg-target-root/bin/twm ]; then
		echo "twm found in build/ -- including it in the rootfs"
		mcopy -i "$ROOTFS_IMG" build/xorg-target-root/bin/twm ::/bin/twm
	fi
	if [ -f build/xorg-target-root/bin/xterm ]; then
		echo "xterm found in build/ -- including it in the rootfs"
		mcopy -i "$ROOTFS_IMG" build/xorg-target-root/bin/xterm ::/bin/xterm
	fi
	if [ -f build/xorg-target-root/bin/xclock ]; then
		echo "xclock found in build/ -- including it in the rootfs"
		mcopy -i "$ROOTFS_IMG" build/xorg-target-root/bin/xclock ::/bin/xclock
	fi
	if [ -f build/xsetbg/xsetbg ]; then
		echo "xsetbg found in build/ -- including it in the rootfs"
		mcopy -i "$ROOTFS_IMG" build/xsetbg/xsetbg ::/bin/xsetbg
	fi
	if [ -f build/xorg-target-root/bin/wmsetbg ]; then
		echo "wmsetbg found in build/ -- including it in the rootfs"
		mcopy -i "$ROOTFS_IMG" build/xorg-target-root/bin/wmsetbg ::/bin/wmsetbg
	fi
	mmd -i "$ROOTFS_IMG" ::/root 2>/dev/null
	mcopy -i "$ROOTFS_IMG" userland/twmrc ::/root/.twmrc
	if [ -f build/xorg-target-root/bin/wmaker ]; then
		echo "wmaker found in build/ -- including it in the rootfs (Phase 36)"
		mcopy -i "$ROOTFS_IMG" build/xorg-target-root/bin/wmaker ::/bin/wmaker
		# WMGLOBAL/WMWindowAttributes/WindowMaker/WMState/WMRootMenu --
		# the system-wide defaults database wmaker reads at startup
		# (configure's --with-pkgconfdir, defaulting to
		# $sysconfdir/WindowMaker = /usr/etc/WindowMaker since this
		# project's X11 components all configure with --prefix=/usr
		# and no separate --sysconfdir).
		if [ -d build/xorg-target-root/usr/etc/WindowMaker ]; then
			mmd -i "$ROOTFS_IMG" ::/usr/etc 2>/dev/null
			mcopy -s -i "$ROOTFS_IMG" build/xorg-target-root/usr/etc/WindowMaker ::/usr/etc/
		fi
		# Backgrounds/Icons/Pixmaps/Styles/Themes/menu -- referenced by
		# path from the WMGLOBAL/WindowMaker defaults files above.
		# ::/usr/share may already exist (created by the xkeyboard-config
		# block above) -- "2>/dev/null" alone doesn't save an mmd on an
		# already-existing dir from also failing its *exit status*, which
		# this script's `set -e` would treat as fatal, so "|| true" too.
		if [ -d build/xorg-target-root/usr/share/WindowMaker ]; then
			mmd -i "$ROOTFS_IMG" ::/usr/share 2>/dev/null || true
			mcopy -s -i "$ROOTFS_IMG" build/xorg-target-root/usr/share/WindowMaker ::/usr/share/
		fi
		# Pre-populate ~/GNUstep/Defaults directly instead of shipping
		# wmaker.inst (upstream's interactive per-user first-run
		# installer script) -- same reasoning/precedent as XFM's ~/.xfm
		# below: this is a single-user OS with no good way to drive an
		# interactive install step before wmaker's first launch.
		# Without this, wmaker's check_defaults() (main.c) can't find
		# ~/GNUstep/Defaults/WindowMaker, tries to run "wmaker.inst
		# --batch" (not present -- "sh: wmaker.inst: not found"), and
		# silently falls back to its hardcoded emergency style with no
		# textures/icon theming -- live-diagnosed via a debug xterm
		# dumping wmaker's own stderr, which printed exactly that
		# warning. The system-wide copies already installed into
		# /usr/etc/WindowMaker above are the same files wmaker.inst
		# would have copied (with the same ~/GNUstep-relative
		# PixmapPath/IconPath entries, which are unaffected since this
		# project's $HOME is genuinely /root), so copying them directly
		# into place is equivalent and skips wmaker.inst entirely.
		if [ -d build/xorg-target-root/usr/etc/WindowMaker ]; then
			mmd -i "$ROOTFS_IMG" ::/root 2>/dev/null || true
			mmd -i "$ROOTFS_IMG" ::/root/GNUstep 2>/dev/null || true
			mmd -i "$ROOTFS_IMG" ::/root/GNUstep/Defaults 2>/dev/null || true
			for f in WindowMaker WMWindowAttributes WMGLOBAL WMState; do
				if [ -f "build/xorg-target-root/usr/etc/WindowMaker/$f" ]; then
					mcopy -i "$ROOTFS_IMG" "build/xorg-target-root/usr/etc/WindowMaker/$f" "::/root/GNUstep/Defaults/$f"
				fi
			done
		fi
	fi
	if [ -f build/xorg-target-root/bin/xfm ]; then
		echo "xfm found in build/ -- including it in the rootfs"
		mcopy -i "$ROOTFS_IMG" build/xorg-target-root/bin/xfm ::/bin/xfm
		if [ -f build/xorg-target-root/bin/xfmtype ]; then
			mcopy -i "$ROOTFS_IMG" build/xorg-target-root/bin/xfmtype ::/bin/xfmtype
		fi
		# bitmaps/pixmaps/icons + app-defaults -- referenced by path from
		# Xfm's own app-defaults file (bitmapPath/pixmapPath).
		if [ -d build/xorg-target-root/usr/share/xfm ]; then
			mmd -i "$ROOTFS_IMG" ::/usr/share 2>/dev/null || true
			mcopy -s -i "$ROOTFS_IMG" build/xorg-target-root/usr/share/xfm ::/usr/share/
		fi
		if [ -f build/xorg-target-root/usr/share/X11/app-defaults/Xfm ]; then
			mmd -i "$ROOTFS_IMG" ::/usr/share 2>/dev/null || true
			mmd -i "$ROOTFS_IMG" ::/usr/share/X11 2>/dev/null || true
			mmd -i "$ROOTFS_IMG" ::/usr/share/X11/app-defaults 2>/dev/null || true
			mcopy -i "$ROOTFS_IMG" build/xorg-target-root/usr/share/X11/app-defaults/Xfm ::/usr/share/X11/app-defaults/Xfm
		fi
		# Pre-populate ~/.xfm directly instead of shipping xfm.install
		# (upstream's interactive first-run setup script) -- this is a
		# single-user, headless-GUI-only OS with no good way to drive an
		# interactive `read` prompt before the file manager's first
		# launch. $HOME is /root (see userland/startx.sh).
		mmd -i "$ROOTFS_IMG" ::/root 2>/dev/null || true
		mmd -i "$ROOTFS_IMG" ::/root/.xfm 2>/dev/null || true
		mmd -i "$ROOTFS_IMG" ::/root/.trash 2>/dev/null || true
		if [ -d build/xorg-target-root/usr/share/xfm/dot.xfm ]; then
			mcopy -i "$ROOTFS_IMG" build/xorg-target-root/usr/share/xfm/dot.xfm/?* ::/root/.xfm/
		fi
	fi
	if [ -f build/xorg-target-root/bin/xeyes ]; then
		echo "xeyes found in build/ -- including it in the rootfs"
		mcopy -i "$ROOTFS_IMG" build/xorg-target-root/bin/xeyes ::/bin/xeyes
	fi
	if [ -f build/xorg-target-root/bin/xedit ]; then
		echo "xedit found in build/ -- including it in the rootfs"
		mcopy -i "$ROOTFS_IMG" build/xorg-target-root/bin/xedit ::/bin/xedit
		if [ -f build/xorg-target-root/usr/share/X11/app-defaults/Xedit ]; then
			mmd -i "$ROOTFS_IMG" ::/usr/share 2>/dev/null || true
			mmd -i "$ROOTFS_IMG" ::/usr/share/X11 2>/dev/null || true
			mmd -i "$ROOTFS_IMG" ::/usr/share/X11/app-defaults 2>/dev/null || true
			mcopy -i "$ROOTFS_IMG" build/xorg-target-root/usr/share/X11/app-defaults/Xedit ::/usr/share/X11/app-defaults/Xedit
			mcopy -i "$ROOTFS_IMG" build/xorg-target-root/usr/share/X11/app-defaults/Xedit-color ::/usr/share/X11/app-defaults/Xedit-color
		fi
		# LISPDIR ($libdir/X11/xedit/lisp = /usr/lib/X11/xedit/lisp) --
		# xedit's lisp/require.c autoloads editing-mode modules from here
		# at runtime (e.g. opening a .c file loads progmodes/c.lsp).
		if [ -d build/xorg-target-root/usr/lib/X11/xedit/lisp ]; then
			mmd -i "$ROOTFS_IMG" ::/usr/lib/X11 2>/dev/null || true
			mmd -i "$ROOTFS_IMG" ::/usr/lib/X11/xedit 2>/dev/null || true
			mcopy -s -i "$ROOTFS_IMG" build/xorg-target-root/usr/lib/X11/xedit/lisp ::/usr/lib/X11/xedit/
		fi
	fi
	if [ -f build/xorg-target-root/bin/xpaint ]; then
		echo "xpaint found in build/ -- including it in the rootfs"
		mcopy -i "$ROOTFS_IMG" build/xorg-target-root/bin/xpaint ::/bin/xpaint
		if [ -f build/xorg-target-root/usr/share/X11/app-defaults/XPaint ]; then
			mmd -i "$ROOTFS_IMG" ::/usr/share 2>/dev/null || true
			mmd -i "$ROOTFS_IMG" ::/usr/share/X11 2>/dev/null || true
			mmd -i "$ROOTFS_IMG" ::/usr/share/X11/app-defaults 2>/dev/null || true
			mcopy -i "$ROOTFS_IMG" build/xorg-target-root/usr/share/X11/app-defaults/XPaint ::/usr/share/X11/app-defaults/XPaint
		fi
		# SHAREDIR (/usr/share/xpaint) -- brushOp.c's brushboxResized()
		# loads bitmaps/brushbox.cfg (and the brushes/elec pattern
		# bitmaps it references) from here at runtime; help/messages
		# are xpaint*helpFile/xpaint*msgFile's own app-defaults paths,
		# both relative to shareDir.
		if [ -d build/xorg-target-root/usr/share/xpaint ]; then
			mmd -i "$ROOTFS_IMG" ::/usr/share 2>/dev/null || true
			mcopy -s -i "$ROOTFS_IMG" build/xorg-target-root/usr/share/xpaint ::/usr/share/
		fi
	fi
	if [ -f build/xorg-target-root/bin/wmiv ]; then
		echo "wmiv found in build/ -- including it in the rootfs"
		mcopy -i "$ROOTFS_IMG" build/xorg-target-root/bin/wmiv ::/bin/wmiv
	fi
fi

DYLD_BIN="build/dyld_obj/dyld"
LIBSYSTEM_REAL="build/libSystem_obj/libSystem.B.dylib"
LIBSYSTEM_PLACEHOLDER="build/dyld_obj/libSystem.B.dylib"
if [ -f "$DYLD_BIN" ]; then
	echo "dyld found in build/ -- including it in the rootfs"
	mcopy -i "$ROOTFS_IMG" "$DYLD_BIN" ::/usr/lib/dyld
	if [ -f "$LIBSYSTEM_REAL" ]; then
		echo "real libSystem.B.dylib found in build/ -- including it in the rootfs"
		mcopy -i "$ROOTFS_IMG" "$LIBSYSTEM_REAL" ::/usr/lib/libSystem.B.dylib
		mcopy -i "$ROOTFS_IMG" build/libSystem_obj/libSystem_selflink_stub.dylib ::/usr/lib/libSystem_selflink_stub.dylib
		if [ -f build/libSystem_obj/systest ]; then
			mcopy -i "$ROOTFS_IMG" build/libSystem_obj/systest ::/bin/systest
		fi
		if [ -f build/libSystem_obj/pthreadtest ]; then
			mcopy -i "$ROOTFS_IMG" build/libSystem_obj/pthreadtest ::/bin/pthreadtest
		fi
		if [ -f build/libSystem_obj/machtest ]; then
			mcopy -i "$ROOTFS_IMG" build/libSystem_obj/machtest ::/bin/machtest
		fi
	elif [ -f "$LIBSYSTEM_PLACEHOLDER" ]; then
		mcopy -i "$ROOTFS_IMG" "$LIBSYSTEM_PLACEHOLDER" ::/usr/lib/libSystem.B.dylib
	fi
	if [ -f build/dyld_obj/libtest.dylib ] && [ -f build/dyld_obj/dyntest ]; then
		mcopy -i "$ROOTFS_IMG" build/dyld_obj/libtest.dylib ::/usr/lib/libtest.dylib
		mcopy -i "$ROOTFS_IMG" build/dyld_obj/dyntest ::/bin/dyntest
	fi
	if [ -f build/libobjc_obj/libobjc.A.dylib ] && [ -f build/libobjc_obj/objctest ]; then
		echo "libobjc found in build/ -- including it in the rootfs"
		mcopy -i "$ROOTFS_IMG" build/libobjc_obj/libobjc.A.dylib ::/usr/lib/libobjc.A.dylib
		mcopy -i "$ROOTFS_IMG" build/libobjc_obj/objctest ::/bin/objctest
	fi
	if [ -f build/helloobjc_obj/helloobjc ]; then
		echo "helloobjc (Phase 27 cross-toolchain regression test) found in build/ -- including it in the rootfs"
		mcopy -i "$ROOTFS_IMG" build/helloobjc_obj/helloobjc ::/bin/helloobjc
		mcopy -i "$ROOTFS_IMG" userland/launchd/daemons/com.asteros.helloobjc.plist ::/etc/launchd/daemons/com.asteros.helloobjc.plist
	fi
	if [ -f build/helloobjc_ontarget_obj/hc_prestaged ]; then
		echo "hc_prestaged (on-target-ld64-built binary, staged pre-built for a fat16lite-freshness isolation test) found in build/ -- including it in the rootfs"
		mcopy -i "$ROOTFS_IMG" build/helloobjc_ontarget_obj/hc_prestaged ::/bin/hc_prestaged
	fi
	if [ -f build/corefoundation_obj/libCoreFoundation.dylib ] && [ -f build/corefoundation_obj/cftest ]; then
		echo "CoreFoundation found in build/ -- including it in the rootfs"
		mcopy -i "$ROOTFS_IMG" build/corefoundation_obj/libCoreFoundation.dylib ::/usr/lib/libCoreFoundation.dylib
		mcopy -i "$ROOTFS_IMG" build/corefoundation_obj/cftest ::/bin/cftest
	fi
	if [ -f build/Foundation_obj/libFoundation.dylib ] && [ -f build/Foundation_obj/foundationtest ]; then
		echo "Foundation found in build/ -- including it in the rootfs"
		mcopy -i "$ROOTFS_IMG" build/Foundation_obj/libFoundation.dylib ::/usr/lib/libFoundation.dylib
		mcopy -i "$ROOTFS_IMG" build/Foundation_obj/foundationtest ::/bin/foundationtest
	fi
	if [ -f build/dispatch_obj/libdispatch.dylib ] && [ -f build/dispatch_obj/dispatchtest ]; then
		echo "libdispatch found in build/ -- including it in the rootfs"
		mcopy -i "$ROOTFS_IMG" build/dispatch_obj/libdispatch.dylib ::/usr/lib/libdispatch.dylib
		mcopy -i "$ROOTFS_IMG" build/dispatch_obj/dispatchtest ::/bin/dispatchtest
	fi
	if [ -f build/security_obj/libSecurity.dylib ] && [ -f build/security_obj/securitytest ]; then
		echo "Security found in build/ -- including it in the rootfs"
		mcopy -i "$ROOTFS_IMG" build/security_obj/libSecurity.dylib ::/usr/lib/libSecurity.dylib
		mcopy -i "$ROOTFS_IMG" build/security_obj/securitytest ::/bin/securitytest
	fi
	if [ -f build/xpc_obj/libxpc.dylib ] && [ -f build/xpc_obj/xpctest ]; then
		echo "libxpc found in build/ -- including it in the rootfs"
		mcopy -i "$ROOTFS_IMG" build/xpc_obj/libxpc.dylib ::/usr/lib/libxpc.dylib
		mcopy -i "$ROOTFS_IMG" build/xpc_obj/xpctest ::/bin/xpctest
	fi
	if [ -f build/configd_obj/configd ]; then
		echo "configd found in build/ -- including it in the rootfs"
		mcopy -i "$ROOTFS_IMG" build/configd_obj/configd ::/sbin/configd
		mcopy -i "$ROOTFS_IMG" userland/launchd/daemons/com.asteros.configd.plist ::/etc/launchd/daemons/com.asteros.configd.plist
		mmd -i "$ROOTFS_IMG" ::/var/tmp
	fi
	if [ -f build/SystemConfiguration_obj/libSystemConfiguration.dylib ] && [ -f build/SystemConfiguration_obj/sctest ]; then
		echo "SystemConfiguration found in build/ -- including it in the rootfs"
		mcopy -i "$ROOTFS_IMG" build/SystemConfiguration_obj/libSystemConfiguration.dylib ::/usr/lib/libSystemConfiguration.dylib
		mcopy -i "$ROOTFS_IMG" build/SystemConfiguration_obj/sctest ::/bin/sctest
		mcopy -i "$ROOTFS_IMG" userland/launchd/daemons/com.asteros.sctest.plist ::/etc/launchd/daemons/com.asteros.sctest.plist
	fi
	if [ -f build/libresolv_obj/libresolv.9.dylib ] && [ -f build/libresolv_obj/restest ]; then
		echo "libresolv found in build/ -- including it in the rootfs"
		mcopy -i "$ROOTFS_IMG" build/libresolv_obj/libresolv.9.dylib ::/usr/lib/libresolv.9.dylib
		mcopy -i "$ROOTFS_IMG" build/libresolv_obj/restest ::/bin/restest
		mcopy -i "$ROOTFS_IMG" userland/launchd/daemons/com.asteros.restest.plist ::/etc/launchd/daemons/com.asteros.restest.plist
	fi
fi

if [ -x "$CLANG_BIN" ] && [ -x "$LD_BIN" ] && [ -f "$LIBCXX" ] && [ -f "$LIBCXXABI" ] \
    && [ -f "$LIBUNWIND" ] && [ -f "$CLANGRT" ]; then
	echo "native toolchain found in build/ -- including it in the rootfs"

	# libc.a is just an archive of the already-built libc objects -- cheap
	# to (re)create here, not a rebuild of anything.
	ar rcs build/libc_obj/libc.a build/libc_obj/*.o

	mmd -i "$ROOTFS_IMG" ::/usr/bin
	mmd -i "$ROOTFS_IMG" ::/usr/include

	mcopy -i "$ROOTFS_IMG" "$CLANG_BIN" ::/usr/bin/clang
	mcopy -i "$ROOTFS_IMG" "$LD_BIN" ::/usr/bin/ld
	mcopy -i "$ROOTFS_IMG" userland/toolchain/clang.cfg ::/usr/bin/clang.cfg
	mcopy -i "$ROOTFS_IMG" build/neatvi_obj/neatvi ::/usr/bin/neatvi

	mcopy -i "$ROOTFS_IMG" build/libc_obj/libc.a ::/usr/lib/libc.a
	mcopy -i "$ROOTFS_IMG" "$LIBCXX" ::/usr/lib/libcxx.a
	mcopy -i "$ROOTFS_IMG" "$LIBCXXABI" ::/usr/lib/libcxxab.a
	mcopy -i "$ROOTFS_IMG" "$LIBUNWIND" ::/usr/lib/libunwnd.a
	mcopy -i "$ROOTFS_IMG" "$CLANGRT" ::/usr/lib/clangrt.a

	mcopy -s -i "$ROOTFS_IMG" userland/libc/include/* ::/usr/include/
	mmd -i "$ROOTFS_IMG" ::/usr/lib/clang
	mmd -i "$ROOTFS_IMG" ::/usr/lib/clang/20
	mcopy -s -i "$ROOTFS_IMG" "$CLANG_RESOURCE_INCLUDE" ::/usr/lib/clang/20/

	# CoreFoundation/Foundation SDK headers: flat, not real .framework
	# bundles (see userland/Foundation/build.sh) -- so no -F search path
	# is needed, just landing them under /usr/include like libc's own
	# headers above, which clang.cfg already -isystem's unconditionally.
	# This is genuinely the missing piece: without it clang -v only ever
	# searches /usr/include and the clang resource dir, and #import
	# <Foundation/Foundation.h> fails with "file not found" even though
	# libFoundation.dylib itself is already on disk at /usr/lib.
	mcopy -s -i "$ROOTFS_IMG" userland/CoreFoundation/include/CoreFoundation ::/usr/include/
	mcopy -s -i "$ROOTFS_IMG" userland/Foundation/include/Foundation ::/usr/include/
	mcopy -s -i "$ROOTFS_IMG" userland/Security/include/Security ::/usr/include/
	mcopy -s -i "$ROOTFS_IMG" userland/libxpc/include/xpc ::/usr/include/

	# crt0.o/libc_start.o as standalone objects, not just archived into
	# libc.a: a dynamically-linked executable (Foundation/CoreFoundation/
	# objc/System dylibs) still needs these two linked in directly by path
	# to get from _start into main -- same recipe as Foundation/test/
	# build.sh's CRT0/LIBC_START, just with the on-target ld in place of
	# the host's.
	mcopy -i "$ROOTFS_IMG" build/libc_obj/crt0.o ::/usr/lib/crt0.o
	mcopy -i "$ROOTFS_IMG" build/libc_obj/libc_start.o ::/usr/lib/libc_start.o
	mcopy -i "$ROOTFS_IMG" build/toolchain_obj/dyld_stub_binder_ref.o ::/usr/lib/dyld_stub_binder_ref.o

	# SDK smoke test: a real .m source file plus the exact clang/ld
	# invocation needed to compile+link it against the just-installed
	# headers/dylibs, run from the guest shell (`sh /tmp/build-hello.sh`)
	# to prove the whole chain end to end, not just that headers parse.
	mcopy -i "$ROOTFS_IMG" userland/Foundation/examples/hello.m ::/tmp/hello.m
	mcopy -i "$ROOTFS_IMG" userland/toolchain/build-hello.sh ::/tmp/build-hello.sh

	# Phase 27 on-target follow-up: same idea, plain libobjc (no
	# Foundation) this time, staged under short names so it's practical
	# to type at the guest shell prompt during QEMU-monitor sendkey
	# verification (`sh /tmp/b.sh`).
	mcopy -i "$ROOTFS_IMG" userland/toolchain/hello_crosscc.m ::/tmp/hello_crosscc.m
	mcopy -i "$ROOTFS_IMG" userland/toolchain/build_hello_crosscc_ontarget.sh ::/tmp/b.sh
else
	echo "no prebuilt native toolchain in build/ -- deploying core rootfs only"
	mcopy -i "$ROOTFS_IMG" build/neatvi_obj/neatvi ::/bin/neatvi
fi

echo "rootfs assembled: $ROOTFS_IMG"
mdir -i "$ROOTFS_IMG" ::
