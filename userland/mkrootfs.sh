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
	echo "pstest found in build/ -- including it in the rootfs"
	mcopy -i "$ROOTFS_IMG" build/pstest/pstest ::/bin/pstest
	mcopy -i "$ROOTFS_IMG" userland/launchd/daemons/com.asteros.pstest.plist ::/etc/launchd/daemons/com.asteros.pstest.plist
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
