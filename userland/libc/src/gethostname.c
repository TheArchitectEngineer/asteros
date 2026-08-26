/* Real Darwin's gethostname(3) is implemented via sysctl(CTL_KERN,
 * KERN_HOSTNAME) -- this project's sysctl() stub (dl_stub.c) only
 * special-cases KERN_OSRELEASE, so rather than grow that MIB table for a
 * single caller, report the same fixed "asteros" identity uname(3)
 * already hardcodes (see uname.c). Added for Phase 30 (real vendored
 * libresolv's res_init.c calls this to seed the default DNS search
 * domain when none is configured). */
#include <unistd.h>
#include <string.h>
#include <errno.h>

int
gethostname(char *name, size_t namelen)
{
	static const char hostname[] = "asteros";

	if (name == NULL) {
		errno = EFAULT;
		return -1;
	}

	strlcpy(name, hostname, namelen);
	return 0;
}
