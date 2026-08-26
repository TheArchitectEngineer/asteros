/* Minimal ifaddrs.h -- getifaddrs()/freeifaddrs() aren't implemented in
 * this project (no SIOCGIFCONF-style interface enumeration yet). Added
 * for Phase 30 so real vendored libresolv's res_send.c can find this
 * header; its own only call site is compiled out unless MULTICAST is
 * defined (which nothing in this project's build does), so the missing
 * implementation is never actually needed. Real struct/prototype shapes
 * kept in case a future phase wants to implement it for real. */
#ifndef _IFADDRS_H_
#define _IFADDRS_H_

#include <sys/cdefs.h>
#include <sys/socket.h>

struct ifaddrs {
	struct ifaddrs  *ifa_next;
	char            *ifa_name;
	unsigned int     ifa_flags;
	struct sockaddr *ifa_addr;
	struct sockaddr *ifa_netmask;
	struct sockaddr *ifa_dstaddr;
	void            *ifa_data;
};

__BEGIN_DECLS

int  getifaddrs(struct ifaddrs **ifap);
void freeifaddrs(struct ifaddrs *ifa);

__END_DECLS

#endif /* _IFADDRS_H_ */
