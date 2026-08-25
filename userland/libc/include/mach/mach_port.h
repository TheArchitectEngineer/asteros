/* Real mach_port_allocate/deallocate/destroy/mod_refs/insert_right --
 * Phase 21 (userland Mach IPC). Thin high-level names over the
 * _kernelrpc_mach_port_*_trap fast-path traps declared in mach_traps.h;
 * implementations in mach_port.c. */
#ifndef _MACH_MACH_PORT_H_
#define _MACH_MACH_PORT_H_

#include <mach/kern_return.h>
#include <mach/message.h>
#include <mach/port.h>

#include <sys/cdefs.h>

__BEGIN_DECLS

extern kern_return_t mach_port_allocate(
	mach_port_name_t target,
	mach_port_right_t right,
	mach_port_name_t *name);

extern kern_return_t mach_port_destroy(
	mach_port_name_t target,
	mach_port_name_t name);

extern kern_return_t mach_port_deallocate(
	mach_port_name_t target,
	mach_port_name_t name);

extern kern_return_t mach_port_mod_refs(
	mach_port_name_t target,
	mach_port_name_t name,
	mach_port_right_t right,
	mach_port_delta_t delta);

extern kern_return_t mach_port_insert_right(
	mach_port_name_t target,
	mach_port_name_t name,
	mach_port_name_t poly,
	mach_msg_type_name_t polyPoly);

extern kern_return_t mach_port_insert_member(
	mach_port_name_t task,
	mach_port_name_t name,
	mach_port_name_t pset);

__END_DECLS

#endif /* _MACH_MACH_PORT_H_ */
