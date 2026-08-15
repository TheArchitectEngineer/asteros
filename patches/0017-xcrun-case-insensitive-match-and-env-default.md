# Patch: deterministic XCRUN routing (case-insensitive SDK match + environment default)

**Problem — `patches/0002`'s `xcrun -sdk <path>` fix worked for one case-spelling
of the repo path, but not the other.** `build/tools/bin/xcrun` (the wrapper
that answers `-sdk <our local SDK copy>` directly since the real
`/usr/bin/xcrun` can't resolve an arbitrary SDK path on this host) recognized
its own SDK via a plain string comparison, `[[ "$sdk_arg" == "$OUR_SDK" ]]`,
against a hardcoded absolute path. This repo's volume is APFS, which is
case-insensitive but case-*preserving*: `cd` into the tree from a
differently-cased path (e.g. a Terminal tab where tab-completion or a typo
landed on `darwinbuildcuzimbore` instead of `DarwinBuildCuzImBore`) still
opens the exact same directory, but every path string derived from that
shell's `pwd` — including `build-kernel.sh`'s own `ROOT`/`SDKROOT` — carries
that different casing forward. The wrapper's string compare then silently
mismatches, falls through to the real `/usr/bin/xcrun`, and every one of the
dozens of `$(shell $(XCRUN) -sdk $(SDKROOT) ...)` calls in
`makedefs/MakeInc.cmd` (used to resolve the SDK path/version *and* to find
every build tool — clang, mig, iig, strip, ...) fails with "SDK ... cannot
be located", exactly as many times as `MakeInc.cmd` gets read across the
build's recursive sub-`make`s. Ground-truthed live, not guessed: reproduced
100% of the time by simply invoking the wrapper from a shell `cd`'d into the
lowercase-spelled path, and gone every time from the mixed-case spelling —
not flaky, not a race, purely case-dependent.

**Fix, part 1 (`build/tools/bin/xcrun`):** match SDK paths by filesystem
identity (`stat -f '%d:%i'`, device+inode) instead of any path *string*.
Two case-spellings of the same directory always report the same
device:inode pair, so this is correct regardless of how the invoking shell
got there. (`cd "$path" && pwd -P` was tried first, on the theory that
physical-path resolution would normalize the case — it does when typed
directly at a shell prompt, but was also ground-truthed live to sometimes
return the *original*, still-differently-cased string when run inside a
script invoked as a fresh `bash somescript` child process instead, an APFS
directory-entry-cache quirk not worth depending on. `stat`'s device:inode
answer doesn't have this problem since it isn't a path string at all.) Also
stopped hardcoding the wrapper's default SDK path to one absolute string
frozen at some prior point in time — it's now derived from the wrapper
script's own location (`build/tools/bin/xcrun` → SDK is two directories up
from `build/`), so moving or renaming the repo doesn't reintroduce the same
class of bug in a new form.

**Fix, part 2 (`makedefs/MakeInc.cmd` + `build-kernel.sh`):** even with the
wrapper itself fixed, `MakeInc.cmd` still hardcoded `XCRUN = /usr/bin/xcrun`
as its default, reached by anything that doesn't get an explicit override —
belt-and-suspenders against any future code path (a recipe that shells out
to a bare `make` instead of `$(MAKE)`, for instance) that might not
inherit a `make XCRUN=...` command-line override the way GNU Make's
documented recursive-invocation propagation normally guarantees.
`MakeInc.cmd`'s `XCRUN` now defaults to `$(DARWINBUILD_XCRUN)` (falling back
to the real `/usr/bin/xcrun` only if that's unset), and `build-kernel.sh`
`export`s `DARWINBUILD_XCRUN=<path to the wrapper>` as a genuine
process-environment variable before any `make` invocation — environment
variables are inherited by *every* child process unconditionally, by the
OS itself, independent of any Make-specific variable-passing mechanism.
Between the two fixes, there is no longer any path — case-spelling,
recursion depth, or invocation style — by which a `make`/`xcrun` call in
this build can reach the real, SDK-path-resolution-broken system `xcrun`
for our own SDK.

**Verified live:** reproduced the original failure from the lowercase-
spelled repo path (`bash build/tools/bin/xcrun -sdk "$(pwd)/build/SDKs/
MacOSX10.15.sdk" -show-sdk-version` → `xcodebuild: error: SDK ... cannot be
located`), confirmed the fixed wrapper resolves it correctly from *both*
case-spellings, then ran a full `bash build-kernel.sh` from the lowercase
path end to end (a genuine clean rebuild, not a no-op — all the way through
`CTFMERGE`/`SYMBOLSET`) with zero `cannot be located` errors, followed by a
full `make run` from the same lowercase path reaching a real QEMU boot with
zero errors.
