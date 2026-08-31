/* SysV shared memory (shmget/shmat/shmdt/shmctl) -- raw BSD syscalls,
 * same private layer syscalls.c uses (see syscall_raw.h). Not
 * previously implemented anywhere in this libc (no src file, the
 * sys/shm.h declarations existed but had no definitions backing
 * them). First real caller: xpaint's magnifier.c, which uses MIT-SHM
 * (libXext's XShm* -- already vendored) for fast pixel readback and
 * calls these directly itself too (alloc_xshm_image()).
 *
 * Syscall numbers 262-265, ground-truthed from
 * src/xnu/bsd/kern/syscalls.master (shmat/shmctl/shmdt/shmget), same
 * sourcing discipline as every other entry in syscall_raw.h -- not
 * guessed. The kernel side (src/xnu/bsd/kern/sysv_shm.c) is real and
 * already vendored/compiled into this project's kernel; this file is
 * the missing userland half.
 */
#include "syscall_raw.h"
#include <sys/shm.h>

#define SYS_shmat  262
#define SYS_shmctl 263
#define SYS_shmdt  264
#define SYS_shmget 265

void *
shmat(int shmid, const void *shmaddr, int shmflg)
{
	long raw = raw_syscall3(SYS_shmat, shmid, (long)shmaddr, shmflg);

	if (g_syscall_cf) {
		errno = (int)raw;
		return (void *)-1;
	}
	return (void *)raw;
}

int
shmctl(int shmid, int cmd, struct shmid_ds *buf)
{
	return (int)sys_result(raw_syscall3(SYS_shmctl, shmid, cmd, (long)buf));
}

int
shmdt(const void *shmaddr)
{
	return (int)sys_result(raw_syscall1(SYS_shmdt, (long)shmaddr));
}

int
shmget(key_t key, size_t size, int shmflg)
{
	return (int)sys_result(raw_syscall3(SYS_shmget, key, (long)size, shmflg));
}
