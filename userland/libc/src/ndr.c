/* Real Apple mach/i386/ndr_def.h (vendored verbatim, see that file)
 * defines -- not just declares -- the NDR_record global real generated
 * MIG client/server stubs reference directly (config.h/configServer.c/
 * configUser.c, Phase 25). Real libsyscall compiles that file directly
 * as source despite its .h extension (it carries no include guard
 * around the definition, by design); this project's libSystem build.sh
 * only globs *.c/*.S out of libc/src, so this trivial wrapper gets it
 * into the build the same way.
 */
#include <mach/i386/ndr_def.h>
