#!/bin/sh
# Launches Xfbdev against this kernel's own framebuffer/input devices --
# /fbdev/fb0 (fbdevfs, Phase 31) and /dev/psevent (Phase 32), bridged
# into the X server by hw/kdrive/fbdev/asteros_input.c (the X11
# milestone's DDX driver) -- then twm as the window manager.
set -e

export DISPLAY=:0

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
# (needs neither) stands in for both. A real limitation (a slow XKB
# compile could still lose this race) but enough for this milestone's
# live verification.
outer="xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
while [ -n "$outer" ]; do
    inner="xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    while [ -n "$inner" ]; do
        inner="${inner#?}"
    done
    outer="${outer#?}"
done

/bin/twm &

wait $XPID
