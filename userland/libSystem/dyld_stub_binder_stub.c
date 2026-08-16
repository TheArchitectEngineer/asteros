/* Copyright (c) 2026 Vihaan Nathan
 *
 * Real Darwin's libSystem.dylib exports `_dyld_stub_binder` (the
 * ordinary, underscore-mangled C name) -- the real runtime lazy-bind
 * entry point, reachable the normal way any dylib-exported C function
 * is. Kept here for SDK completeness/accuracy even though it turned out
 * not to be what AsterOS's own self-hosted ld64 actually needs at link
 * time -- see userland/toolchain/dyld_stub_binder_ref.S for the real
 * fix and the full story (short version: ld64's own internal "well-
 * known atom" lookup for this mechanism is keyed on the *unmangled*
 * literal name `dyld_stub_binder`, no underscore, a completely
 * different symbol-table entry from this one -- ground-truthed the
 * expensive way, empirically, after this export alone didn't fix
 * anything). If this ever actually runs it means something reached it
 * through an ordinary `-lSystem`-style reference rather than through
 * the special linker-synthesized path -- fail loudly either way, since
 * real lazy binding isn't wired up anywhere in this project (every
 * binary links -bind_at_load, see TODO.md Phase 11's "known v1
 * limitations").
 */
#include <stdio.h>
#include <stdlib.h>

void
dyld_stub_binder(void)
{
	fprintf(stderr, "dyld_stub_binder: real lazy binding is not supported "
	    "(every AsterOS binary is linked -bind_at_load)\n");
	abort();
}
