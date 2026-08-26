/* ____chkstk_darwin -- the stack-probe helper clang/LLVM's X86 backend
 * emits a call to whenever a single function's stack frame (typically a
 * large alloca()) is big enough that just subtracting the whole size
 * from %rsp in one shot could jump clean over the guard page below the
 * stack, defeating stack-overflow detection. Not something project code
 * calls directly -- the compiler inserts the call on its own, so this
 * has to exist under the exact name the codegen references (confirmed
 * empirically via the real undefined-symbol linker error this project
 * hit vendoring libXfont2/zlib for the X11 milestone: four leading
 * underscores, not the three a naive reading of LLVM's own "__chkstk_darwin"
 * disassembly text would suggest -- ground-truthed against the actual
 * error, not assumed).
 *
 * ABI (ground-truthed against llvm/test/CodeGen/X86/probe-stack-eflags.ll,
 * which exists specifically to pin this down): called with the requested
 * allocation size in %rax; must probe every 4K page from the caller's
 * real %rsp down to (that %rsp - %rax) so each one individually faults
 * the kernel's stack-growth machinery if it's actually a guard page,
 * rather than skipping over it; must NOT touch %rsp itself (the caller
 * does `subq %rax, %rsp` immediately after the call) and must preserve
 * %rax and the flags register across the call (the eflags-live test
 * above exists precisely because some callers have a live flags value
 * spanning the call). Modeled on compiler-rt's own x86_64/chkstk.S
 * (___chkstk_ms, the Windows/MinGW equivalent) with an added pushfq/
 * popfq pair for the flags-preservation requirement that routine
 * doesn't need. */
	.text

	.globl ____chkstk_darwin
____chkstk_darwin:
	push	%rcx
	push	%rax
	pushfq
	cmp	$0x1000, %rax
	lea	32(%rsp), %rcx
	jb	1f
2:
	sub	$0x1000, %rcx
	test	%rcx, (%rcx)
	sub	$0x1000, %rax
	cmp	$0x1000, %rax
	ja	2b
1:
	sub	%rax, %rcx
	test	%rcx, (%rcx)
	popfq
	pop	%rax
	pop	%rcx
	ret
