/* Real nftw(): a genuine recursive directory walk built on top of
 * this libc's existing opendir/readdir/lstat/stat, not a stub --
 * WindowMaker's WINGs/proplist.c uses it for real directory-hierarchy
 * removal (wrmdirhier()), so a fake result would just move the bug.
 * fd_limit (the max number of simultaneously open directories) is
 * accepted for signature compatibility but not enforced -- one DIR*
 * per recursion level, closed before returning from that level, is
 * already the natural bound and this libc has no fd-pressure concern
 * worth adding bookkeeping for.
 */
#include <ftw.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

static int ftw_walk(char *path, size_t pathlen, __ftw_fn fn, int flags, int level)
{
	struct stat st;
	struct FTW ftw;
	int is_dir, err;
	int (*statfn)(const char *, struct stat *) = (flags & FTW_PHYS) ? lstat : stat;

	if (statfn(path, &st) == -1) {
		ftw.base = 0;
		ftw.level = level;
		return fn(path, &st, (errno == 0) ? FTW_NS : FTW_NS, &ftw);
	}

	is_dir = S_ISDIR(st.st_mode);

	ftw.level = level;
	{
		char *slash = strrchr(path, '/');
		ftw.base = slash ? (int)(slash - path + 1) : 0;
	}

	if (is_dir) {
		DIR *dirp = opendir(path);

		if (!dirp)
			return fn(path, &st, FTW_DNR, &ftw);

		if (!(flags & FTW_DEPTH)) {
			err = fn(path, &st, FTW_D, &ftw);
			if (err) {
				closedir(dirp);
				return err;
			}
		}

		for (;;) {
			struct dirent *de = readdir(dirp);
			size_t namelen;

			if (!de)
				break;
			if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
				continue;

			namelen = strlen(de->d_name);
			if (pathlen + 1 + namelen + 1 > PATH_MAX) {
				closedir(dirp);
				return fn(path, &st, FTW_NS, &ftw);
			}

			path[pathlen] = '/';
			memcpy(path + pathlen + 1, de->d_name, namelen + 1);

			err = ftw_walk(path, pathlen + 1 + namelen, fn, flags, level + 1);

			path[pathlen] = '\0';

			if (err) {
				closedir(dirp);
				return err;
			}
		}

		closedir(dirp);

		if (flags & FTW_DEPTH)
			return fn(path, &st, FTW_DP, &ftw);

		return 0;
	}

	if (S_ISLNK(st.st_mode))
		return fn(path, &st, FTW_SL, &ftw);

	return fn(path, &st, FTW_F, &ftw);
}

int nftw(const char *path, __ftw_fn fn, int fd_limit, int flags)
{
	char buf[PATH_MAX];
	size_t len = strlen(path);

	(void)fd_limit;

	if (len >= sizeof(buf)) {
		errno = ENAMETOOLONG;
		return -1;
	}

	memcpy(buf, path, len + 1);

	/* Strip a single trailing slash (but not "/" itself) so path
	 * concatenation below doesn't produce a double slash.
	 */
	if (len > 1 && buf[len - 1] == '/') {
		buf[len - 1] = '\0';
		len--;
	}

	return ftw_walk(buf, len, fn, flags, 0);
}
