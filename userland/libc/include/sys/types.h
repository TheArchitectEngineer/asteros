/* Minimal sys/types.h for our no-dyld Darwin libc shim.
 * Field widths ground-truthed against src/xnu/bsd/sys headers for xnu-6153
 * (x86_64, __DARWIN_64_BIT_INO_T==1, __DARWIN_UNIX03==1 -- see
 * docs/architecture.md / patches for how that was confirmed) -- NOT the
 * generic POSIX text, since what matters here is matching this exact
 * kernel's syscall ABI.
 */
#ifndef _SYS_TYPES_H_
#define _SYS_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
/* Real Darwin's own sys/types.h transitively provides FD_SETSIZE/fd_set
 * this way too -- added for Phase 30 (real vendored libresolv uses
 * FD_SETSIZE after only #include <sys/types.h>, matching real practice). */
#include <sys/_types/_fd_def.h>
#include <sys/_types/_fd_setsize.h>
#include <sys/_types/_fd_set.h>
#include <sys/_types/_fd_clr.h>
#include <sys/_types/_fd_isset.h>
#include <sys/_types/_fd_zero.h>

typedef long ssize_t;

/* Real Darwin guards off_t's definition behind _OFF_T so that whichever
 * of sys/types.h or sys/_types/_off_t.h (the real, vendored split-header
 * version some real Apple headers -- e.g. sys/fcntl.h -- include
 * directly) gets included first wins, instead of the second one
 * redefining the typedef with a different underlying type and erroring.
 * This project's own off_t here didn't have that guard (Phase 30
 * surfaced it: real vendored libresolv pulls in sys/fcntl.h). */
#ifndef _OFF_T
#define _OFF_T
typedef long off_t;
#endif /* _OFF_T */
typedef int pid_t;
typedef unsigned short mode_t;
typedef unsigned short nlink_t;
typedef unsigned long long ino_t;
typedef unsigned long long ino64_t;
typedef int dev_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef long blkcnt_t;
typedef int blksize_t;
typedef unsigned int fflags_t;
typedef long time_t;
#include <sys/_types/_suseconds_t.h> /* __darwin_suseconds_t, guarded, matches struct timeval */
typedef unsigned int useconds_t;
typedef long clock_t;
typedef unsigned long tcflag_t;
typedef unsigned char cc_t;
typedef unsigned long speed_t;
typedef unsigned int sigset_t;
typedef unsigned long u_long;
typedef unsigned int u_int;
typedef unsigned short u_short;
typedef unsigned char u_char;
typedef long key_t;
typedef char *caddr_t;
/* BSD legacy fixed-width aliases (src/xnu/bsd/sys/_types/_u_intNN_t.h) --
 * distinct from <stdint.h>'s uintN_t (no underscore): several vendored
 * xnu netinet headers (ip_icmp.h, if.h) use these directly. */
typedef unsigned char u_int8_t;
typedef unsigned short u_int16_t;
typedef unsigned int u_int32_t;
typedef unsigned long long u_int64_t;

#ifdef __cplusplus
}
#endif

#endif /* _SYS_TYPES_H_ */
