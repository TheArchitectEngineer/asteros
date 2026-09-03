#!/bin/sh
# Launches Xfbdev against this kernel's own framebuffer/input devices --
# /fbdev/fb0 (fbdevfs, Phase 31) and /dev/psevent (Phase 32), bridged
# into the X server by hw/kdrive/fbdev/asteros_input.c (the X11
# milestone's DDX driver) -- then wmaker (WindowMaker, Phase 36) as the
# window manager.
# No `set -e`: every step below is best-effort so one slow/failing
# client never blocks the rest of the desktop from coming up.

export DISPLAY=:0
export HOME=/root
# /bin/sh -c needs PATH to resolve bare command names, so set this
# regardless -- the WMRootMenu EXEC/SHEXEC "Could not execute command"
# bug (WindowMaker's own fork()+exec() chain, see TODO.md Phase 36
# follow-up) is now fixed (commit 5672c4f), so this is just an ordinary
# PATH export again, not a workaround for that.
# /usr/bin holds clang/ld/neatvi (see userland/mkrootfs.sh's
# native-toolchain block) -- missing here meant every client this script
# launches (and everything they in turn fork/exec, e.g. an xterm shell)
# inherited a PATH that could `mcopy` these binaries onto the disk image
# but never actually resolve them by bare name, always needing
# `/usr/bin/clang` spelled out in full. Found live: `clang -v`/`neatvi`
# in an xterm both failed with "not found" even though `/usr/bin/clang
# -v` ran fine.
export PATH=/bin:/sbin:/usr/bin

# libXt's compiled-in default app-defaults search path is stale -- it
# bakes in this repo's absolute build path from when it was still named
# DarwinBuildCuzImBore (see build/xorg-deps-install/lib/libXt.a's own
# embedded XFILESEARCHPATH strings), so it points at a directory that no
# longer exists on disk. Every X11 client here is affected, but most
# (twm/xterm/xclock) just silently fall back to built-in resource
# defaults when their app-defaults file can't be found -- xfm is the
# first one to treat a missing app-defaults file as fatal (its own
# appDefsVersion-mismatch check in FmMain.c pops a real "Sorry: Appl.
# Defaults Not Found" dialog instead of degrading quietly). Setting
# XFILESEARCHPATH here overrides Xt's broken compiled-in default with
# the real, current install location (see src/xfm/build.sh's install
# step / userland/mkrootfs.sh) for every client startx.sh launches.
export XFILESEARCHPATH="/usr/share/X11/%T/%N%C:/usr/share/X11/%T/%N"

# -nolock: LockServer() (os/utils.c) uses link() as its atomic
# single-instance check, which fat16lite can't support (no hard links
# on FAT) -- always fails with "Can't read lock file". There's only
# ever one display/one X server on this single-user OS, so the check
# has no purpose here anyway.
/bin/Xfbdev :0 -nolock &
XPID=$!

echo "Xfbdev started, pid $XPID, DISPLAY=$DISPLAY"

# No xinit-style connection-retry loop available yet, and this
# busybox build has neither sleep/usleep (CONFIG_SLEEP off) nor ash
# arithmetic expansion ($((...)) -- CONFIG_ASH_MATH_SUPPORT off) --
# a busy-wait using POSIX "#?"" parameter-expansion string-shrinking
# (needs neither) stands in for both.
outer="xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
while [ -n "$outer" ]; do
    inner="xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    while [ -n "$inner" ]; do
        inner="${inner#?}"
    done
    outer="${outer#?}"
done

# Solid red fill first (userland/xsetbg.c, instant) so the root window
# is never seen unpainted between Xfbdev's own default background and
# the real wallpaper below. Best-effort: don't let this block wmaker
# from launching if it fails or hangs.
/bin/xsetbg 192 0 0 &

# Real photo wallpaper via wmsetbg (WindowMaker's own bg-setting
# utility, linked against real libjpeg -- see src/libjpeg/build.sh)
# -f/--fillscale scales the image to fill the screen while keeping its
# aspect ratio, matching how a normal desktop wallpaper behaves.
/bin/wmsetbg -f /usr/share/WindowMaker/Backgrounds/Flowers.jpg &

# Deliberately no auto-launched xterm/xclock/anything else here.
# WMRootMenu already has an ("XTerm", EXEC, "xterm -sb") entry, so the
# user opens one from WindowMaker's own root menu (right-click on bare
# desktop) when they actually want one. This is also sidestepping a
# real, still-unresolved bug rather than papering over it: an
# auto-launched client here (xclock in an earlier version of this
# file) would reliably lose a race against WindowMaker's own
# SubstructureRedirect registration and come up permanently
# undecorated -- reproduced with several different mitigations
# (longer waits, extra decoy clients, a real CPU-yielding `read -t`
# wait) that all failed to fix it. With no client auto-launched, there
# is nothing left to race, so the symptom cannot occur; it wasn't
# actually fixed. Investigating WindowMaker's own startup sequence
# (not this script) is the real fix, left for a future pass.
/bin/wmaker &

# xcompmgr (Phase 40) -- DISABLED for now, real regression found.
# Off-screen redirection via Composite (re-enabled in Xfbdev this
# phase; was previously built with --disable-composite) and the
# compositing-manager machinery itself were proven genuinely live
# (confirmed with a DEBUG_REPAINT-instrumented rebuild: paint_all()
# was compositing every window every frame; a numerically-confirmed
# real shadow was measured on both a WindowMaker Dock icon (-c) and a
# real xterm (-s), see git history/TODO.md Phase 40 for the full
# story). But running `-s` live surfaced a real, uninvestigated
# regression: WindowMaker's Dock icons (Clip and friends) render
# washed-out/pale -- barely any shading -- instead of their real
# XPM/PNG artwork, confirmed by comparing raw pixel values from before
# xcompmgr ever ran (real shading, values spanning ~118-255) against
# after. Not root-caused yet -- leading suspicion is a window-pixmap
# staleness/generation-tracking gap between xcompmgr's `w->pixmap`
# caching (`XCompositeNameWindowPixmap`, only ever fetched once per
# window, xcompmgr.c's `if (hasNamePixmap && !w->pixmap)`) and this
# project's brand-new, never-before-exercised Composite implementation
# in Xfbdev/kdrive -- but that's a guess, not confirmed. Composite
# stays enabled at the server level (real infrastructure, no
# regression there); xcompmgr itself is commented out below until this
# is actually root-caused, since a washed-out desktop is a worse
# tradeoff than missing shadows.
# /bin/xcompmgr -s &

wait $XPID
