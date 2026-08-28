# Third-party licenses

AsterOS's own original code (see `LICENSE`, EUPL-1.2) sits on top of a
large number of real, pinned upstream source trees under `src/`. Each
of those keeps its own original license — vendoring code into this
repo doesn't relicense it. This file is a map of what's under `src/`
and what license actually governs it; where the upstream project ships
its own license file, that file is the authoritative text, not this
summary.

## Apple Public Source License 2.0 (APSL-2.0)

Real Apple/`apple-oss-distributions` source, unmodified except where a
patch under `patches/` or a live kernel fix says otherwise. Full text
in each directory's `APPLE_LICENSE` file where present, or at
<https://www.opensource.apple.com/apsl/>.

`xnu`, `dtrace`, `ld64`, `bootstrap_cmds`, `configd`, `Libsystem`,
`CommonCrypto`, `AvailabilityVersions`, `libDER`, `libpthread`,
`libdispatch`, `libplatform`

## GNU General Public License v2.0 (GPL-2.0)

`busybox` — see `src/busybox/LICENSE`. Distributed as a separate
binary (`/bin/busybox` and its applet symlinks/copies in the rootfs),
not statically or dynamically linked into anything else in this
project.

## BSD-style (4-clause, Regents of the University of California)

`libresolv` — see the license header in `src/libresolv/resolv.h`.

## ISC License

`neatvi` — Copyright (C) Ali Gholami Rudi. See the header in
`src/neatvi/vi.c`. Permissive, no share-alike requirement.

## MIT/X11 License

The X.org ecosystem — protocols, libraries, and apps — all under the
standard X11/MIT license. Each directory below ships its own
`COPYING` (or, for `libxpm`, `COPYING` + `COPYRIGHT`) with the exact
text:

`bigreqsproto`, `damageproto`, `fixesproto`, `font-util`, `fontsproto`,
`inputproto`, `kbproto`, `libXau`, `libXfont2`, `libfontenc`, `libice`,
`libsm`, `libx11`, `libxaw`, `libxcb`, `libxdmcp`, `libxext`,
`libxkbfile`, `libxmu`, `libxpm`, `libxt`, `pixman`, `pthread-stubs`,
`randrproto`, `renderproto`, `twm`, `xcbproto`, `xclock`,
`xcmiscproto`, `xextproto`, `xkbcomp`, `xkeyboard-config`,
`xorg-server`, `xorg-util-macros`, `xproto`, `xterm`, `xtrans`

## MIT License

`musl-math-src` — musl libc's math functions, MIT-licensed. See
`src/musl-math-src/COPYRIGHT`.

## Apache License 2.0 (with LLVM exceptions)

`llvm-project` — see `src/llvm-project/LICENSE.TXT`.

## zlib License

`zlib` — see `src/zlib/LICENSE`.

## If something's missing or wrong here

This list was built by checking each `src/` directory for its own
license file or file-header copyright notice, not from memory of what
each project "usually" uses — but it's still a manually-maintained
summary, not a substitute for the license each upstream project
actually ships. If you're relying on this for anything beyond casually
browsing the repo, verify against the license file in the specific
directory you care about.
