/* Copyright (c) 2026 Vihaan Nathan
 *
 * Defines the literal symbol `dyld_stub_binder` -- deliberately with no
 * leading underscore, unlike every normal C symbol on this ABI. Real
 * Darwin's own ld64 treats this one name as a special, hardcoded,
 * *unmangled* linker-internal symbol: the `_fast_lazy_bind` slot every
 * dynamic executable's stub-helper section gets is eagerly bound to it
 * by name (ground-truthed against userland/dyld/stub_binder_asm.S's own
 * header comment, itself ground-truthed against
 * src/ld64/src/ld/passes/stubs/stub_x86_64.hpp: "gets eagerly bound ...
 * symbol name `dyld_stub_binder`" -- no underscore). AsterOS's own
 * self-hosted ld64 (userland/ld64_shim, see TODO.md Phase 27's
 * on-target follow-up) has its own hardcoded, matching, no-underscore
 * lookup for this exact name in Resolver.cpp's
 * fillInHelpersInInternalState() -- but its search for a dylib
 * exporting it fails even when one genuinely does (ground-truthed
 * empirically: adding a real, exported, ordinary C `dyld_stub_binder`
 * symbol -- which mangles to `_dyld_stub_binder`, an entirely different
 * symbol-table entry from this one -- to libSystem.B.dylib and to
 * several directly-linked objects never satisfied it, because none of
 * those attempts were ever the symbol this specific check is named
 * after).
 *
 * Providing this exact name as a real, always-linked-in definition
 * sidesteps needing ld64's search to work at all: it's simply already
 * defined, the same way it would be if the search had succeeded.
 * Its own body is never actually reached: the real lazy-call path this
 * feeds is genuinely dead code given -bind_at_load everywhere in this
 * project (see build_hello_crosscc_ontarget.sh) -- traps instead of
 * returning if it ever somehow is.
 *
 * A hand-written .S file, not C: C's own name-mangling convention on
 * this ABI always adds the leading underscore no compiler flag turns
 * off, so this specific, deliberately non-standard name can only be
 * produced directly in assembly.
 */
	.text
	.globl	dyld_stub_binder
dyld_stub_binder:
	ud2
