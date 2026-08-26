/* getifaddrs()/freeifaddrs() aren't implemented in this project -- no
 * SIOCGIFCONF-style interface enumeration exists yet (see ifaddrs.h).
 * Added for Phase 30 so real vendored libresolv's res_send.c links --
 * its only call site is already guarded to handle getifaddrs() failing,
 * so an honest ENOSYS is correct here, not a placeholder. */
#include <ifaddrs.h>
#include <errno.h>
#include <stddef.h>

int
getifaddrs(struct ifaddrs **ifap)
{
	*ifap = NULL;
	errno = ENOSYS;
	return -1;
}

void
freeifaddrs(struct ifaddrs *ifa)
{
	(void)ifa;
}
