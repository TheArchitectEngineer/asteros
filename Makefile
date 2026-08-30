# Top-level build orchestration for AsterOS.
# `make run` builds everything it needs and boots the result in QEMU.
# See docs/architecture.md / TODO.md for what each piece actually is.

ROOT        := $(abspath .)
CLANG       := /Applications/Xcode-beta.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang
TOOLS_BIN   := $(ROOT)/build/tools/bin
NPROC       := $(shell sysctl -n hw.ncpu)
export PATH := $(TOOLS_BIN):$(PATH)

QEMU        := qemu-system-x86_64
OVMF_CODE   := boot/OVMF_CODE.fd
OVMF_VARS   := boot/OVMF_VARS.fd
ESP_IMG     := boot/esp.img
ROOTFS_IMG  := boot/fat16.img

KERNEL_BIN  := build/kernel/kernel.development
BOOTX64     := boot/BOOTX64.EFI
LIBC_STAMP  := build/libc_obj/.stamp
BUSYBOX_BIN := src/busybox/busybox_unstripped
NEATVI_BIN  := build/neatvi_obj/neatvi
LAUNCHD_BIN := build/launchd/launchd
LAUNCHCTL_BIN := build/launchctl_obj/launchctl

TOOLCHAIN_CLANG := build/llvm-static-build/bin/clang
TOOLCHAIN_LD    := build/ld64_bin/ld64

.PHONY: all kernel bootloader libc busybox neatvi launchd launchctl toolchain image run clean help kernel-build busybox-build check-stale-qemu

all: image

help:
	@echo "targets: all, kernel, bootloader, libc, busybox, neatvi, launchd, launchctl, toolchain, image, run, clean"

# --- kernel -----------------------------------------------------------
# `kernel-build` always delegates to build-kernel.sh, which itself calls
# into xnu's own incremental `make` -- fast (a no-op check) when nothing
# under src/xnu changed -- and only touches $(KERNEL_BIN)'s mtime when
# the resulting kernel.development actually differs (see build-kernel.sh's
# cmp-before-cp). $(KERNEL_BIN)'s own recipe is a no-op: it exists purely
# so downstream targets (like $(ESP_IMG)) depend on this file's *real*
# mtime rather than on the always-stale phony `kernel-build`, so `make
# run` doesn't reassemble the image when nothing actually changed.
kernel: $(KERNEL_BIN)
kernel-build:
	./build-kernel.sh
$(KERNEL_BIN): kernel-build
	@:

# --- bootloader ---------------------------------------------------------
bootloader: $(BOOTX64)
$(BOOTX64): boot/boot.c boot/darwin_boot.h boot/mach_o.h boot/efi.h boot/transition.S boot/build.sh
	cd boot && bash build.sh

# --- libc shim ----------------------------------------------------------
libc: $(LIBC_STAMP)
$(LIBC_STAMP): $(wildcard userland/libc/src/*.c userland/libc/src/*.S userland/libc/src/musl_math/*.c) \
               $(wildcard userland/libc/include/*.h) userland/libc/build.sh
	cd userland/libc && bash build.sh
	touch $@

# --- busybox --------------------------------------------------------------
# Real prerequisites, not an always-rerun phony (unlike kernel/busybox's
# own two-level split above): busybox's final static link turned out to be
# non-deterministic byte-for-byte (relinking identical, unchanged .o/.a
# inputs still produces a different binary each time -- almost certainly
# ar/ld embedding something like a timestamp), so a cmp-before-replace
# guard inside link_manual.sh can't tell "really changed" from "just
# relinked" the way build-kernel.sh's kernel.development one can. Gating
# on real file mtimes here instead means busybox only rebuilds when this
# project's own libc actually changes (busybox is vendored, upstream
# source this project doesn't edit) -- `make busybox-build` still forces
# a manual relink if ever needed.
busybox: $(BUSYBOX_BIN)
busybox-build:
	$(MAKE) -C src/busybox CC="$(TOOLS_BIN)/cc-nogroup" AR="$(TOOLS_BIN)/ar" -j$(NPROC) || true
	cd src/busybox && ROOT="$(ROOT)" bash link_manual.sh
$(BUSYBOX_BIN): $(LIBC_STAMP)
	$(MAKE) -C src/busybox CC="$(TOOLS_BIN)/cc-nogroup" AR="$(TOOLS_BIN)/ar" -j$(NPROC) || true
	cd src/busybox && ROOT="$(ROOT)" bash link_manual.sh

# --- neatvi ---------------------------------------------------------------
neatvi: $(NEATVI_BIN)
$(NEATVI_BIN): $(wildcard src/neatvi/*.c src/neatvi/*.h) $(LIBC_STAMP)
	cd src/neatvi && bash build.sh
	cd src/neatvi && bash link.sh

# --- launchd (PID 1) ---------------------------------------------------
# Real xnu's load_init_program() tries /sbin/launchd before /sbin/init
# (ground-truthed in src/xnu/bsd/kern/kern_exec.c), so this ships at
# /sbin/launchd -- see userland/mkrootfs.sh.
launchd: $(LAUNCHD_BIN)
$(LAUNCHD_BIN): userland/launchd/launchd.c userland/launchd/plist.c userland/launchd/plist.h \
    userland/launchd/bootstrap_server.c userland/launchd/bootstrap_server.h \
    userland/launchd/control_server.c userland/launchd/control_server.h \
    userland/launchd/launchd_control.h userland/launchd/launchd_control_client.c \
    userland/launchd/launchd_ops.h $(LIBC_STAMP)
	mkdir -p build/launchd
	$(CLANG) -target x86_64-apple-macos10.15 -ffreestanding -fno-stack-protector -fno-builtin \
	    -nostdlibinc -isystem userland/libc/include -O1 -g \
	    -c userland/launchd/launchd.c -o build/launchd/launchd.o
	$(CLANG) -target x86_64-apple-macos10.15 -ffreestanding -fno-stack-protector -fno-builtin \
	    -nostdlibinc -isystem userland/libc/include -O1 -g \
	    -c userland/launchd/plist.c -o build/launchd/plist.o
	$(CLANG) -target x86_64-apple-macos10.15 -ffreestanding -fno-stack-protector -fno-builtin \
	    -nostdlibinc -isystem userland/libc/include -O1 -g \
	    -c userland/launchd/bootstrap_server.c -o build/launchd/bootstrap_server.o
	$(CLANG) -target x86_64-apple-macos10.15 -ffreestanding -fno-stack-protector -fno-builtin \
	    -nostdlibinc -isystem userland/libc/include -O1 -g \
	    -c userland/launchd/control_server.c -o build/launchd/control_server.o
	$(CLANG) -target x86_64-apple-macos10.15 -ffreestanding -fno-stack-protector -fno-builtin \
	    -nostdlibinc -isystem userland/libc/include -O1 -g \
	    -c userland/launchd/launchd_control_client.c -o build/launchd/launchd_control_client.o
	$(CLANG) -target x86_64-apple-macos10.15 -nostdlib -static -e _start \
	    build/launchd/launchd.o build/launchd/plist.o build/launchd/bootstrap_server.o \
	    build/launchd/control_server.o build/launchd/launchd_control_client.o \
	    build/libc_obj/*.o -o $(LAUNCHD_BIN)

# --- launchctl (launchd's control CLI) ---------------------------------
launchctl: $(LAUNCHCTL_BIN)
$(LAUNCHCTL_BIN): userland/launchctl/launchctl.c userland/launchctl/build.sh \
    userland/launchd/launchd_control.h userland/launchd/launchd_control_client.c $(LIBC_STAMP)
	bash userland/launchctl/build.sh

# --- native toolchain (never built here -- see docs/architecture.md) ------
# LLVM/clang/ld64 are cross-built by hand over many hours (Phase 10 in
# TODO.md); this Makefile only ever *packages* whatever's already sitting
# in build/ (userland/mkrootfs.sh does the actual copying into the image).
toolchain:
	@if [ -x "$(TOOLCHAIN_CLANG)" ] && [ -x "$(TOOLCHAIN_LD)" ]; then \
		echo "prebuilt native toolchain present in build/ -- image will include it"; \
	else \
		echo "no prebuilt native toolchain in build/ -- image will be core-only"; \
		echo "(building one from scratch is a separate, long-running process -- see TODO.md Phase 10)"; \
	fi

# --- disk images ------------------------------------------------------
# mkrootfs.sh/mkesp.sh rebuild boot/fat16.img and boot/esp.img from
# scratch (rm -f + reformat) on every invocation, which means a QEMU
# process from an earlier `make run` that's still holding either file
# open (mmap'd disk backing, or just not yet exited) can make the
# mtools calls that write over it hang indefinitely with zero output --
# live-hit once as a silent stall right after mkrootfs.sh's own
# "wmaker found..." echo, no error, no further progress, no indication
# anything was even still running. check-stale-qemu catches the most
# likely cause *before* the rebuild even starts; run_with_heartbeat
# below is the fallback for any other cause, so a genuine stall is at
# least visibly a stall rather than indistinguishable from a crashed
# terminal.
#
# An earlier version of this fix tried to wrap every individual mtools
# call in mkrootfs.sh/mkesp.sh with a kill-after-N-seconds timeout.
# Twice. Both attempts introduced a *new*, worse hang of their own
# (confirmed by A/B timing against the unwrapped script: 0.7s clean vs.
# 90-120s with either "fix" in place) -- old bash (macOS's /bin/bash is
# 3.2, no `wait -n`/`timeout`) has no clean way to reap a specific
# background job with a deadline: `kill -0 $pid` keeps reporting a
# finished-but-unreaped child as "alive" (it's a zombie, not gone)
# until something actually calls `wait` on it, so a poll loop built on
# `kill -0` never observes completion on its own; and a background
# "sleep N; kill ..." subshell watchdog can be killed without killing
# the `sleep` it's blocked in, leaking an orphan that lingers for the
# rest of N holding this script's own stdout fd open. Chasing a second
# fix for the second bug is exactly the kind of complexity this file
# doesn't need for what's fundamentally a "tell me it's still alive"
# problem, not a "forcibly kill it" problem -- so: a single one-shot
# heartbeat around the *whole* script instead, with no loop and no
# retry, so there's nothing left running for it to leak.
check-stale-qemu:
	@stale="$$(pgrep -fl 'qemu-system-x86_64.*(esp\.img|fat16\.img)' 2>/dev/null || true)"; \
	if [ -n "$$stale" ]; then \
		echo "warning: a qemu-system-x86_64 process is still holding boot/esp.img or boot/fat16.img open:"; \
		echo "$$stale" | sed 's/^/  /'; \
		echo "  If the image rebuild below hangs, this is almost certainly why -- stop that process first (kill <pid>, or Ctrl-C its terminal) and re-run."; \
	fi

# $1 = human-readable description, $2.. = command to run. A normal run
# of either mkrootfs.sh or mkesp.sh takes under a second, so one
# reminder at 60s is plenty -- this is a single `sleep 60`, not a loop,
# specifically so there's no repeated-sleep orphan for a stray `kill`
# to leak: even in the worst case where the kill below doesn't land in
# time, the absolute most that can be left behind is one already-timed
# `sleep` process quietly finishing on its own a few seconds later.
run_with_heartbeat = \
	( sleep 60; echo "note: '$(1)' has been running over 60s -- if it looks stuck, check for a stray qemu-system-x86_64 process holding boot/fat16.img or boot/esp.img open (pgrep -fl qemu-system-x86_64), kill it, and re-run." >&2 ) & \
	heartbeat_pid=$$!; \
	$(2); status=$$?; \
	kill $$heartbeat_pid >/dev/null 2>&1; \
	exit $$status

# Both rules below depend on real output files ($(BUSYBOX_BIN),
# $(KERNEL_BIN), etc.), not the phony `busybox`/`kernel` target names --
# mkrootfs.sh/mkesp.sh always rebuild their image from scratch when
# invoked (see mkrootfs.sh's header comment on why: repeated in-place
# mcopy fragments the FAT cluster chain), so `make run` only re-invokes
# them when a prerequisite's mtime genuinely changed, not on every run.
image: $(ESP_IMG)

$(ROOTFS_IMG): $(BUSYBOX_BIN) $(NEATVI_BIN) $(LAUNCHD_BIN) $(LAUNCHCTL_BIN) | check-stale-qemu
	@$(call run_with_heartbeat,userland/mkrootfs.sh,bash userland/mkrootfs.sh)

$(ESP_IMG): $(BOOTX64) $(KERNEL_BIN) $(ROOTFS_IMG) | check-stale-qemu
	@$(call run_with_heartbeat,boot/mkesp.sh,bash boot/mkesp.sh)

# --- run ----------------------------------------------------------------
# -serial mon:stdio multiplexes the serial console and the QEMU monitor
# onto the same terminal (Ctrl-A C toggles to the monitor and back) --
# this is where kernel -v boot messages (kprintf) land. The interactive
# shell itself lives on the QEMU window's own display (GOP framebuffer
# console, serial=2 in boot.c's cmdline is input-only) -- that window is
# where you actually type commands, not the terminal running this target.
run: image
	$(QEMU) -machine q35 -cpu Haswell -m 2048 \
	    -drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
	    -drive if=pflash,format=raw,file=$(OVMF_VARS) \
	    -drive format=raw,file=$(ESP_IMG) \
	    -netdev user,id=net0 -device virtio-net-pci,netdev=net0 \
	    -serial mon:stdio -no-reboot -no-shutdown

# --- clean ----------------------------------------------------------------
# Removes this Makefile's own products so `make`/`make run` regenerates
# them from scratch. Leaves build/SDKs, the xnu BUILD/ tree, and any
# built native toolchain alone -- those are expensive prerequisites this
# Makefile doesn't own and never rebuilds on its own.
clean:
	rm -f $(KERNEL_BIN) $(BOOTX64) boot/boot.o boot/transition.o
	rm -rf build/libc_obj build/neatvi_obj build/launchd build/launchctl_obj
	-$(MAKE) -C src/busybox clean
	rm -f $(BUSYBOX_BIN)
	rm -f $(ROOTFS_IMG) $(ESP_IMG)
