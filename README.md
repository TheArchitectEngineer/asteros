# AsterOS

<img width="642" height="431" alt="image" src="https://github.com/user-attachments/assets/f0618ab5-b655-4968-894c-5849a294dcb7" />

A from-source bring-up of a minimal Darwin 19 (Catalina, xnu-6153.141.1) x86_64
system: real XNU kernel + BSD/Mach, a custom UEFI bootloader (Apple's own
`boot.efi` isn't open source), a real userland — dyld, libSystem, libobjc,
launchd, real pthreads, CoreFoundation/Foundation/libdispatch/Security/libxpc,
prelinked kext loading, networking, and an X11 desktop (Xfbdev + twm + xterm +
xclock) — booting to an interactive shell and GUI in QEMU.

See `docs/architecture.md` for the full set of design decisions and why they
were made. See `patches/` for every source patch applied to the upstream
Apple/X.org sources, each with a rationale. See `logs/` for build/boot logs.
See `TODO.md` for the full phase-by-phase build log and current status —
it's long, but it's the authoritative record of what's actually done versus
in progress.

## Layout

- `src/` — pinned checkouts of every vendored upstream project this system
  is built from: XNU and other `apple-oss-distributions` components, the
  X.org stack (`xorg-server`, `libX11`, `twm`, `xterm`, `xclock`, and their
  dependencies), BusyBox, LLVM, and a handful of smaller pieces. See
  `THIRD_PARTY_LICENSES.md` for what's licensed how.
- `userland/` — this project's own userland: the libc, launchd/launchctl,
  build scripts for every vendored/native component, and small original
  tools (`xsetbg`, `startx.sh`, etc).
- `boot/` — the custom EFI bootloader source and disk-image assembly
  scripts (`mkesp.sh`).
- `build/` — all build output. `build/SDKs/MacOSX10.15.sdk` is a local,
  header-patched copy of the host's SDK (no real Catalina SDK exists on this
  host); `build/tools/bin` holds host build tools including an `xcrun` shim
  and the ctf tools.
- `patches/` — one markdown file per patch, numbered, each self-contained.
- `docs/` — architecture notes.
- `logs/` — build and boot logs.

## Building and running

```
make run
```

Builds the kernel, bootloader, userland, and root filesystem, assembles the
ESP disk image, and boots it in QEMU. See the `Makefile` for individual
targets (`kernel`, `image`, `clean`, etc) if you want to build a specific
piece. `build-kernel.sh` is the older, lower-level entry point for just the
kernel bring-up phase (ctf tools, `exporthdrs`, then the kernel proper into
`build/kernel/kernel.development`) — see `patches/0001`-`0020.md` for what
it took to get a stock XNU tree building against a modern host toolchain.

Once booted, `startx` brings up the X11 desktop (twm window manager, three
`xterm`s, `xclock`).

## License

This project's own original code — the bootloader, kernel patches that
aren't themselves modifications of vendored upstream source, userland
tools, build scripts, and documentation — is licensed under the
**European Union Public Licence v1.2 (EUPL-1.2)**, a strong-copyleft
license: see `LICENSE`.

Everything under `src/` is a real, pinned checkout of someone else's
project and keeps its own original license (APSL-2.0 for XNU and other
Apple components, GPL-2.0 for BusyBox, MIT/X11 for the X.org stack, and
a few others) — see `THIRD_PARTY_LICENSES.md` for the full breakdown.
