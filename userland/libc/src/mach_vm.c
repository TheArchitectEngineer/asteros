/* Real vm_allocate/vm_deallocate -- userland wrappers over the
 * _kernelrpc_mach_vm_{allocate,deallocate}_trap fast-path Mach traps
 * (src/xnu/osfmk/kern/syscall_sw.c:115,117), same shape as
 * mach_port.c's mach_port_allocate/destroy. Needed for Phase 25's
 * SystemConfiguration/configd: MIG's `xmlDataOut, dealloc` out-of-line
 * data convention (config.defs) expects a vm_allocate'd buffer handed
 * back to the generated server stub, which vm_deallocate's it after the
 * reply is sent -- a plain malloc'd pointer isn't safe to pass to
 * vm_deallocate (it expects a real VM region, not a heap allocation).
 */
#include <mach/kern_return.h>
#include <mach/vm_map.h>
#include <mach/vm_types.h>
#include <mach/port.h>

#include "mach_trap_raw.h"

kern_return_t
vm_allocate(
	vm_map_t target,
	vm_address_t *address,
	vm_size_t size,
	int flags)
{
	return (kern_return_t)raw_mach_trap4(MACH_TRAP_kernelrpc_mach_vm_allocate,
	    (long)target, (long)address, (long)size, (long)flags);
}

kern_return_t
vm_deallocate(
	vm_map_t target,
	vm_address_t address,
	vm_size_t size)
{
	return (kern_return_t)raw_mach_trap3(MACH_TRAP_kernelrpc_mach_vm_deallocate,
	    (long)target, (long)address, (long)size);
}
