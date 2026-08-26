#!/bin/sh
# Launches Xfbdev against this kernel's own framebuffer/input devices --
# /fbdev/fb0 (fbdevfs, Phase 31) and /dev/psevent (Phase 32), bridged
# into the X server by hw/kdrive/fbdev/asteros_input.c (the X11
# milestone's DDX driver). No window manager is started yet (twm is a
# later phase) -- this just proves the server itself comes up and draws
# to the real screen.
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
wait $XPID
