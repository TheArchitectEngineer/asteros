/* Real mach_msg()/mach_msg_trap()/mach_msg_overwrite_trap() plus the
 * task/thread/host self-port traps and mach_reply_port(). First userland
 * Mach IPC in this tree -- see mach_trap_raw.h for the raw asm layer and
 * TODO.md's Phase 21 writeup for the full design rationale.
 *
 * mach_task_self_ (the backing store for the mach_task_self() macro in
 * mach/mach_init.h) is defined here and initialized once per process from
 * __libc_start (libc_start.c) -- same "shared global, per-executable init
 * hook" split already used for `environ`.
 *
 * Deliberately does NOT include mach/mach_init.h: that header #defines
 * `mach_task_self()` as a zero-arg function-like macro
 * (`mach_task_self() mach_task_self_`), which would rewrite this file's
 * own `mach_task_self(void)` FUNCTION DEFINITION below into a malformed
 * macro invocation ("void" read as an argument to a 0-arg macro) --
 * caught by the compiler, not guessed. Both forms still work correctly
 * project-wide: TUs that include mach_init.h get the macro (a direct read
 * of the `mach_task_self_` global, no call); TUs that include mach/mach.h
 * instead (dl_stub.c, pthread.c) get a plain extern declaration and link
 * against the real function defined here. */
#include <mach/mach_traps.h>
#include <mach/message.h>
#include <mach/port.h>

#include "mach_trap_raw.h"

mach_port_t mach_task_self_ = MACH_PORT_NULL;
mach_port_t bootstrap_port = MACH_PORT_NULL;

void
__mach_init_task_self(void)
{
	mach_task_self_ = (mach_port_t)task_self_trap();
}

/* A real callable function, not just the mach_task_self() macro
 * (mach/mach_init.h's `#define mach_task_self() mach_task_self_`) --
 * existing callers in this tree (pthread.c, previously dl_stub.c's now-
 * removed stub) reach this through mach/mach.h's plain
 * `mach_port_t mach_task_self(void);` declaration instead, so both forms
 * need to resolve correctly depending on which header a given TU
 * included. Replaces dl_stub.c's old `return 1;` placeholder. */
mach_port_t
mach_task_self(void)
{
	return mach_task_self_;
}

mach_port_name_t
task_self_trap(void)
{
	return (mach_port_name_t)raw_mach_trap0(MACH_TRAP_task_self_trap);
}

mach_port_t
mach_thread_self(void)
{
	return (mach_port_t)raw_mach_trap0(MACH_TRAP_thread_self_trap);
}

mach_port_t
mach_host_self(void)
{
	return (mach_port_t)raw_mach_trap0(MACH_TRAP_host_self_trap);
}

mach_port_name_t
mach_reply_port(void)
{
	return (mach_port_name_t)raw_mach_trap0(MACH_TRAP_mach_reply_port);
}

/* Both mach_msg_trap and mach_msg_overwrite_trap resolve to the exact
 * same kernel implementation -- ground-truthed by reading
 * src/xnu/osfmk/ipc/mach_msg.c:710-718: mach_msg_trap() is literally
 * `args->rcv_msg = 0; return mach_msg_overwrite_trap(args);`. Both
 * "possibly send a message; possibly receive a message" atomically in
 * ONE call when both MACH_SEND_MSG and MACH_RCV_MSG are set (not two
 * separate operations) -- so this always issues a single
 * mach_msg_overwrite_trap, passing rcv_msg=0 (meaning "use msg for both
 * send and receive") when no distinct receive buffer was given, matching
 * the real mach_msg_trap's own behavior exactly. */
mach_msg_return_t
mach_msg_overwrite(
	mach_msg_header_t *msg,
	mach_msg_option_t option,
	mach_msg_size_t send_size,
	mach_msg_size_t rcv_size,
	mach_port_name_t rcv_name,
	mach_msg_timeout_t timeout,
	mach_port_name_t notify,
	mach_msg_header_t *rcv_msg,
	mach_msg_size_t rcv_limit)
{
	(void)notify; /* dropped from the trap args in this xnu era -- see
	               * mach_trap_raw.h's MACH_TRAP_mach_msg_overwrite_trap
	               * comment; the trap's 7th slot is a priority override,
	               * not a notify port. Kept as a parameter only for
	               * mach_msg_overwrite()'s public signature parity. */
	(void)rcv_limit;

	return (mach_msg_return_t)raw_mach_trap8(MACH_TRAP_mach_msg_overwrite_trap,
	    (long)msg, (long)option, (long)send_size, (long)rcv_size,
	    (long)rcv_name, (long)timeout, (long)MACH_MSG_PRIORITY_UNSPECIFIED,
	    (long)rcv_msg);
}

mach_msg_return_t
mach_msg(
	mach_msg_header_t *msg,
	mach_msg_option_t option,
	mach_msg_size_t send_size,
	mach_msg_size_t rcv_size,
	mach_port_name_t rcv_name,
	mach_msg_timeout_t timeout,
	mach_port_name_t notify)
{
	return mach_msg_overwrite(msg, option, send_size, rcv_size, rcv_name,
	    timeout, notify, MACH_MSG_NULL, 0);
}
