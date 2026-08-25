# Patch: vendor Apple's real `mig` compiler as a host build tool

**Why.** Phase 25 (SystemConfiguration/configd) needs to compile
`SystemConfiguration.fproj/config.defs` — a genuine MIG interface, not a
hand-marshaled one — to stay faithful to the real wire protocol rather
than reinterpreting it. `mig` isn't already vendored anywhere in this
project; it lives in a separate Apple project, `bootstrap_cmds/
migcom.tproj`, not `configd` itself.

**What.** `src/bootstrap_cmds/` is a new plain nested git repo (same
non-submodule convention as `src/xnu`), cloned from
`apple-oss-distributions/bootstrap_cmds` at tag `bootstrap_cmds-121`
(Catalina/10.15.6-vintage, matching this project's existing
`xnu-6153.141.1`/`libdispatch-1173.100.2`/`Libsystem-1281.100.1` pins —
not independently date-verified beyond tag-family adjacency; if a future
build against this project's other Catalina-era components ever surfaces
a MIG-protocol mismatch, re-check this specific tag choice first).

`userland/toolchain/mig/build.sh` builds `migcom` (the real compiler
binary, `bison`+`flex`+11 vendored `.c` files) as a **host** tool — it
runs on the Mac doing the build, generating C from `.defs` files at build
time, the same shape as `userland/toolchain/kextbuild/kxld_link_tool.c`
and `prelink_merge.py`. It never runs on the target OS; the target only
ever sees the plain C files `migcom` generates.

**Two real bugs found and fixed while building it, both ground-truthed
against the vendored project file, not guessed:**

1. `handler.c` fails to compile against the rest of this source drop
   (references `IsCamelot`/`IsKernel`/`rtMaxReplySize`/`itDeallocate`/
   `itLongForm` — fields/globals that don't exist anywhere else in this
   same tag). Grepping `mig.xcodeproj/project.pbxproj`'s actual `Sources`
   build phase directly confirms `handler.c` is listed in the project's
   file *group* but was never in the real compiled target — dead code
   left in the tree, not something this project broke. Excluded from the
   build.
2. `lexxer.l`'s own `#include "y.tab.h"` expects bison's *traditional*
   yacc output naming, not bison's own default (`parser.tab.c`/`.h`) —
   `build.sh` passes `-o y.tab.c` explicitly to match.

`MIG_VERSION` (a build-setting-supplied string in the real Xcode project,
`-DMIG_VERSION=\"$(RC_ProjectNameAndSourceVersion)\"`) is hardcoded to
the literal vendored tag string, `"bootstrap_cmds-121"`.

**Installed as** `build/tools/bin/migcom` + `build/tools/bin/mig` — the
latter is Apple's real `mig.sh` wrapper (preprocesses a `.defs` file with
the C preprocessor, pipes the result into `migcom`), copied verbatim
except for one line: its default `migcomPath` (normally computed relative
to the script's own location, `../libexec/migcom`) is hardcoded to this
project's flat `build/tools/bin/` layout instead, matching how every
other host tool here (`xcrun`, `cc-nogroup`) already lives.

**Verified real, not just "it compiled":** a trivial `.defs` file
(`#include <mach/std_types.defs>`/`<mach/mach_types.defs>` + one
`routine`) run through the installed `build/tools/bin/mig` produces a
valid `.h`/`User.c`/`Server.c` triple with real MIG boilerplate
(`mig_internal kern_return_t __MIG_check__Reply__..._t(...)`, real
NDR/voucher includes) — the genuine compiler pipeline end to end, not a
stub.
