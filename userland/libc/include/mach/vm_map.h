/* Real Darwin's classic (pre-mach_vm_*) VM allocation API -- vm_allocate/
 * vm_deallocate, backed by the same _kernelrpc_mach_vm_{allocate,
 * deallocate}_trap traps mach_vm_allocate/mach_vm_deallocate use (this
 * project is x86_64-only, so there's no 32-bit-vs-64-bit distinction to
 * make between the two API families the way real multi-arch Darwin has).
 * See userland/libc/src/mach_vm.c for the implementation.
 */
#ifndef _MACH_VM_MAP_H_
#define _MACH_VM_MAP_H_

#include <mach/kern_return.h>
#include <mach/vm_types.h>
#include <mach/vm_statistics.h>

#ifdef __cplusplus
extern "C" {
#endif

kern_return_t vm_allocate(vm_map_t target, vm_address_t *address, vm_size_t size, int flags);
kern_return_t vm_deallocate(vm_map_t target, vm_address_t address, vm_size_t size);

#ifdef __cplusplus
}
#endif

#endif /* _MACH_VM_MAP_H_ */
