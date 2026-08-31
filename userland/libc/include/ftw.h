/* Real Darwin/POSIX ftw.h -- struct FTW and the FTW_ constants and
 * nftw()'s own flags below match the standard SUSv3 numbering used by
 * every real libc (glibc, BSD libc, Darwin's own). Needed by
 * WindowMaker's WINGs/proplist.c (wrmdirhier(), recursive directory
 * removal).
 */
#ifndef _FTW_H_
#define _FTW_H_

#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

struct FTW {
	int base;
	int level;
};

/* passed as the "type" argument to the nftw() callback */
#define FTW_F   0	/* regular file */
#define FTW_D   1	/* directory, preorder */
#define FTW_DNR 2	/* directory that could not be read */
#define FTW_NS  3	/* stat() failed, no info available */
#define FTW_SL  4	/* symbolic link */
#define FTW_DP  5	/* directory, postorder (FTW_DEPTH only) */
#define FTW_SLN 6	/* symbolic link pointing to a nonexistent file */

/* flags for nftw()'s own 4th argument */
#define FTW_PHYS   0x01	/* physical walk, do not follow symlinks */
#define FTW_MOUNT  0x02	/* stay within the starting filesystem */
#define FTW_DEPTH  0x04	/* postorder: visit a directory's contents before the directory itself */
#define FTW_CHDIR  0x08	/* chdir() into each directory while walking */

typedef int (*__ftw_fn)(const char *, const struct stat *, int, struct FTW *);

int nftw(const char *path, __ftw_fn fn, int fd_limit, int flags);

#ifdef __cplusplus
}
#endif

#endif /* _FTW_H_ */
