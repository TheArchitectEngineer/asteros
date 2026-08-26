/* Real Darwin's sys/_types/_fd_def.h declares this as weak_import --
 * real dyld resolves an absent weak import to NULL at load time and the
 * inline caller (__darwin_check_fd_set in that same header) branches on
 * that. This project's static, -bind_at_load linking has no equivalent
 * leniency (same class of gap already solved once this session for
 * voucher_mach_msg_set), so a real definition is required to link at
 * all. This project's fd_set is always the fixed __DARWIN_FD_SETSIZE
 * shape (no _DARWIN_UNLIMITED_SELECT support), so plain bounds-checking
 * against that fixed size is the honest, correct answer. */
#include <sys/_types/_fd_def.h>

int
__darwin_check_fd_set_overflow(int fd, const void *fdset, int is_set)
{
	(void)fdset;
	(void)is_set;
	return fd >= 0 && fd < __DARWIN_FD_SETSIZE;
}
