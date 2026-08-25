/* Real Darwin's fileport_makeport/fileport_makefd -- wraps a file
 * descriptor in a Mach port so it can be handed to another process over
 * Mach IPC (real syscalls 430/431, ground-truthed against
 * src/xnu/bsd/kern/syscalls.master:665-666). Needed for Phase 25's
 * config.defs `notifyviafd` routine, which passes a `fileport` argument
 * this way. */
#ifndef _SYS_FILEPORT_H_
#define _SYS_FILEPORT_H_

#include <mach/port.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef mach_port_t fileport_t;

int fileport_makeport(int fd, mach_port_t *portnamep);
int fileport_makefd(mach_port_t port);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_FILEPORT_H_ */
