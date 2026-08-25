/* Private raw Mach trap layer -- class-1 syscalls ((1 << 24) | number in
 * %rax, per src/xnu/osfmk/mach/i386/syscall_sw.h's SYSCALL_CLASS_MACH == 1
 * shifted by SYSCALL_CLASS_SHIFT == 24), same six argument registers as
 * the BSD class-2 raw layer (syscall_raw.h) -- rdi,rsi,rdx,r10,r8,r9 (r10
 * not rcx -- `syscall` clobbers rcx with the return address) -- because
 * both classes are dispatched from the exact same `syscall` instruction
 * entry point (src/xnu/osfmk/x86_64/idt64.s:1715-1744 branches on the
 * class bits in %rax to hndl_mach_scall64 vs hndl_unix_scall64), just
 * different handler functions once inside the kernel.
 *
 * Unlike BSD traps, Mach traps do NOT use the carry-flag error
 * convention: mach_call_munger64() (src/xnu/osfmk/i386/bsd_i386.c:653)
 * writes the raw kern_return_t straight into %rax with no carry-flag
 * signaling at all -- ground-truthed by reading the dispatcher, not
 * guessed. Every wrapper here just returns %rax directly.
 *
 * Not installed as a public header -- internal to libc/src only. */
#ifndef DARWINBUILD_MACH_TRAP_RAW_H
#define DARWINBUILD_MACH_TRAP_RAW_H

#define MACH_TRAP_NUM(n) ((1u << 24) | (unsigned)(n))

/* mach_reply_port, thread_self_trap, task_self_trap, host_self_trap */
#define MACH_TRAP_mach_reply_port      26
#define MACH_TRAP_thread_self_trap     27
#define MACH_TRAP_task_self_trap       28
#define MACH_TRAP_host_self_trap       29

#define MACH_TRAP_mach_msg_trap            31
#define MACH_TRAP_mach_msg_overwrite_trap  32

#define MACH_TRAP_kernelrpc_mach_port_allocate       16
#define MACH_TRAP_kernelrpc_mach_port_destroy        17
#define MACH_TRAP_kernelrpc_mach_port_deallocate     18
#define MACH_TRAP_kernelrpc_mach_port_mod_refs       19
#define MACH_TRAP_kernelrpc_mach_port_insert_right   21
#define MACH_TRAP_kernelrpc_mach_port_insert_member  22
#define MACH_TRAP_kernelrpc_mach_vm_allocate         10
#define MACH_TRAP_kernelrpc_mach_vm_deallocate       12

static inline long
raw_mach_trap0(unsigned num)
{
	long ret;
	__asm__ __volatile__("syscall"
	    : "=a"(ret)
	    : "a"(MACH_TRAP_NUM(num))
	    : "rcx", "r11", "memory");
	return ret;
}

static inline long
raw_mach_trap2(unsigned num, long a1, long a2)
{
	long ret;
	register long r_a1 __asm__("rdi") = a1;
	register long r_a2 __asm__("rsi") = a2;
	__asm__ __volatile__("syscall"
	    : "=a"(ret)
	    : "a"(MACH_TRAP_NUM(num)), "r"(r_a1), "r"(r_a2)
	    : "rcx", "r11", "memory");
	return ret;
}

static inline long
raw_mach_trap3(unsigned num, long a1, long a2, long a3)
{
	long ret;
	register long r_a1 __asm__("rdi") = a1;
	register long r_a2 __asm__("rsi") = a2;
	register long r_a3 __asm__("rdx") = a3;
	__asm__ __volatile__("syscall"
	    : "=a"(ret)
	    : "a"(MACH_TRAP_NUM(num)), "r"(r_a1), "r"(r_a2), "r"(r_a3)
	    : "rcx", "r11", "memory");
	return ret;
}

static inline long
raw_mach_trap4(unsigned num, long a1, long a2, long a3, long a4)
{
	long ret;
	register long r_a1 __asm__("rdi") = a1;
	register long r_a2 __asm__("rsi") = a2;
	register long r_a3 __asm__("rdx") = a3;
	register long r_a4 __asm__("r10") = a4;
	__asm__ __volatile__("syscall"
	    : "=a"(ret)
	    : "a"(MACH_TRAP_NUM(num)), "r"(r_a1), "r"(r_a2), "r"(r_a3), "r"(r_a4)
	    : "rcx", "r11", "memory");
	return ret;
}

/* mach_msg_trap: 7 real args (msg, option, send_size, rcv_size, rcv_name,
 * timeout, override) -- the last needs a 7th slot beyond the 6 argument
 * registers. xnu's generic argument copyin (mach_call_munger64,
 * src/xnu/osfmk/i386/bsd_i386.c:635, and the identical convention in
 * bsd/dev/i386/systemcalls.c for BSD syscalls) reads anything beyond the
 * 6 register args from the user stack starting at `isf.rsp + 8`, mirroring
 * where a `call`-based stub's real args would start after its pushed
 * return address -- a bare `syscall` never pushes one, so a throwaway
 * dummy word must occupy that skipped slot.
 *
 * IMPORTANT, found the hard way while writing this: push order matters
 * and is easy to get backwards. The LAST `pushq` executed ends up at the
 * LOWEST address (current %rsp, i.e. +0); whatever was pushed earlier
 * ends up at the HIGHER address (+8, +16, ...). To land the dummy at
 * [rsp+0] and the real extra argument(s) at [rsp+8] (ascending), the real
 * argument(s) must be pushed FIRST and the dummy pushed LAST -- the
 * opposite order from what a naive reading of "push the dummy, then push
 * the real arg" suggests. (The existing BSD raw_syscall7() in
 * syscall_raw.h pushes the dummy first and the real arg second, which by
 * this same trace lands the dummy at [rsp+8] and the real arg at [rsp+0]
 * -- backwards from what xnu actually reads. It has gone unnoticed only
 * because its one caller, bsdthread_register()'s tsd_offset, is always
 * legitimately 0 in this tree -- see TODO.md Phase 16 -- so misreading
 * the dummy instead of the real arg is unobservable. Flagged separately,
 * not fixed here since it's outside this phase's scope.) */
static inline long
raw_mach_trap7(unsigned num, long a1, long a2, long a3, long a4, long a5, long a6, long a7)
{
	long ret;
	register long r_a1 __asm__("rdi") = a1;
	register long r_a2 __asm__("rsi") = a2;
	register long r_a3 __asm__("rdx") = a3;
	register long r_a4 __asm__("r10") = a4;
	register long r_a5 __asm__("r8") = a5;
	register long r_a6 __asm__("r9") = a6;
	__asm__ __volatile__(
	    "pushq %8\n\t"      /* real arg7 -- pushed first, ends at [rsp+8] after the dummy push below */
	    "pushq $0\n\t"      /* dummy return-address slot -- pushed last, ends at [rsp+0] */
	    "syscall\n\t"
	    "addq $16, %%rsp"
	    : "=a"(ret)
	    : "a"(MACH_TRAP_NUM(num)), "r"(r_a1), "r"(r_a2), "r"(r_a3), "r"(r_a4), "r"(r_a5), "r"(r_a6), "g"(a7)
	    : "rcx", "r11", "memory", "cc");
	return ret;
}

/* mach_msg_overwrite_trap: 8 real args -- same convention as
 * raw_mach_trap7 above, extended by one more stack slot. Real args
 * pushed first (in reverse order, so the last-pushed real arg ends up
 * closest to the dummy, i.e. at [rsp+8], and the first-pushed real arg
 * ends up at [rsp+16]), dummy pushed last (lands at [rsp+0]). */
static inline long
raw_mach_trap8(unsigned num, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8)
{
	long ret;
	register long r_a1 __asm__("rdi") = a1;
	register long r_a2 __asm__("rsi") = a2;
	register long r_a3 __asm__("rdx") = a3;
	register long r_a4 __asm__("r10") = a4;
	register long r_a5 __asm__("r8") = a5;
	register long r_a6 __asm__("r9") = a6;
	__asm__ __volatile__(
	    "pushq %9\n\t"      /* real arg8 -- pushed first, ends at [rsp+16] */
	    "pushq %8\n\t"      /* real arg7 -- pushed second, ends at [rsp+8] */
	    "pushq $0\n\t"      /* dummy -- pushed last, ends at [rsp+0] */
	    "syscall\n\t"
	    "addq $24, %%rsp"
	    : "=a"(ret)
	    : "a"(MACH_TRAP_NUM(num)), "r"(r_a1), "r"(r_a2), "r"(r_a3), "r"(r_a4), "r"(r_a5), "r"(r_a6), "g"(a7), "g"(a8)
	    : "rcx", "r11", "memory", "cc");
	return ret;
}

#endif /* DARWINBUILD_MACH_TRAP_RAW_H */
