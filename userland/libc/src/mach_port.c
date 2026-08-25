/* Real mach_port_allocate/deallocate/destroy/mod_refs/insert_right --
 * userland wrappers over the _kernelrpc_mach_port_*_trap fast-path Mach
 * traps (src/xnu/osfmk/kern/syscall_sw.c:121-126), which are genuine
 * traps, not MIG routines, so no message marshaling is needed here
 * (unlike mach_special_ports.c's task_get/set_special_port). See
 * mach_trap_raw.h for the raw asm layer.
 */
#include <mach/kern_return.h>
#include <mach/mach_port.h>
#include <mach/message.h>
#include <mach/port.h>

#include "mach_trap_raw.h"

kern_return_t
mach_port_allocate(
	mach_port_name_t target,
	mach_port_right_t right,
	mach_port_name_t *name)
{
	return (kern_return_t)raw_mach_trap3(MACH_TRAP_kernelrpc_mach_port_allocate,
	    (long)target, (long)right, (long)name);
}

kern_return_t
mach_port_destroy(
	mach_port_name_t target,
	mach_port_name_t name)
{
	return (kern_return_t)raw_mach_trap2(MACH_TRAP_kernelrpc_mach_port_destroy,
	    (long)target, (long)name);
}

kern_return_t
mach_port_deallocate(
	mach_port_name_t target,
	mach_port_name_t name)
{
	return (kern_return_t)raw_mach_trap2(MACH_TRAP_kernelrpc_mach_port_deallocate,
	    (long)target, (long)name);
}

kern_return_t
mach_port_mod_refs(
	mach_port_name_t target,
	mach_port_name_t name,
	mach_port_right_t right,
	mach_port_delta_t delta)
{
	return (kern_return_t)raw_mach_trap4(MACH_TRAP_kernelrpc_mach_port_mod_refs,
	    (long)target, (long)name, (long)right, (long)delta);
}

/* poly/polyPoly: the port name whose right is being inserted, and the
 * disposition (MACH_MSG_TYPE_MAKE_SEND etc.) of that right -- ground-
 * truthed against mach/mach_traps.h's _kernelrpc_mach_port_insert_right_trap
 * declaration. The common use here: a task that holds a RECEIVE right at
 * `name` calls this with poly=name, polyPoly=MACH_MSG_TYPE_MAKE_SEND to
 * derive a SEND right at the same name in its own space -- the standard
 * "receiver mints its own send right" idiom Phase 21's bootstrap design
 * relies on. */
kern_return_t
mach_port_insert_right(
	mach_port_name_t target,
	mach_port_name_t name,
	mach_port_name_t poly,
	mach_msg_type_name_t polyPoly)
{
	return (kern_return_t)raw_mach_trap4(MACH_TRAP_kernelrpc_mach_port_insert_right,
	    (long)target, (long)name, (long)poly, (long)polyPoly);
}

/* Real trap 22 (src/xnu/osfmk/kern/syscall_sw.c:127,
 * _kernelrpc_mach_port_insert_member_trap, 3 word args) -- adds `name`
 * (a receive right) as a member of port set `pset`, so a single
 * mach_msg(MACH_RCV_MSG) on `pset` delivers whichever member port's
 * message actually arrived. Needed for Phase 25's configd, which fans
 * in one receive port per open SCDynamicStore session this way instead
 * of real configd's per-session CFRunLoopSource (CFRunLoop doesn't exist
 * in this project). */
kern_return_t
mach_port_insert_member(
	mach_port_name_t task,
	mach_port_name_t name,
	mach_port_name_t pset)
{
	return (kern_return_t)raw_mach_trap3(MACH_TRAP_kernelrpc_mach_port_insert_member,
	    (long)task, (long)name, (long)pset);
}
