/* DNS/hostname resolution still has no backing resolver in this kernel
 * config (see docs/architecture.md) -- those stay stubbed below. The
 * inet_pton/ntop/addr/aton/ntoa family, though, is pure string<->binary
 * format conversion with no kernel dependency at all, so as of the
 * loopback-TCP/IP milestone (Phase 24) these are real, not stubs -- IPv4
 * (AF_INET) only, matching that phase's static-IP-only scope; AF_INET6
 * honestly reports EAFNOSUPPORT rather than pretending to parse. The
 * actual socket syscalls (socket/bind/connect/accept/send/recv/...) are
 * real too, implemented in socket.c, generic across address families. */
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

int h_errno;
const char *hstrerror(int err) { (void)err; return "network not supported"; }

struct hostent *gethostbyname(const char *name) { (void)name; return (void *)0; }
struct hostent *gethostbyaddr(const void *addr, size_t len, int type) { (void)addr; (void)len; (void)type; return (void *)0; }
int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res)
{
	(void)node; (void)service; (void)hints; (void)res;
	return -1;
}
void freeaddrinfo(struct addrinfo *res) { (void)res; }
const char *gai_strerror(int errcode) { (void)errcode; return "network not supported"; }
struct servent *getservbyname(const char *name, const char *proto) { (void)name; (void)proto; return (void *)0; }
struct servent *getservbyport(int port, const char *proto) { (void)port; (void)proto; return (void *)0; }

const char *
inet_ntop(int af, const void *src, char *dst, unsigned int size)
{
	if (af != AF_INET) {
		errno = EAFNOSUPPORT;
		return (void *)0;
	}
	const unsigned char *b = (const unsigned char *)src;
	char buf[16];
	int n = snprintf(buf, sizeof(buf), "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
	if (n < 0 || (unsigned int)n >= size) {
		errno = ENOSPC;
		return (void *)0;
	}
	memcpy(dst, buf, (size_t)n + 1);
	return dst;
}

int
inet_pton(int af, const char *src, void *dst)
{
	if (af != AF_INET) {
		errno = EAFNOSUPPORT;
		return -1;
	}
	unsigned int parts[4];
	int n = 0;
	const char *p = src;
	for (int i = 0; i < 4; i++) {
		if (i > 0) {
			if (*p != '.') return 0;
			p++;
		}
		if (*p < '0' || *p > '9') return 0;
		unsigned int v = 0;
		int digits = 0;
		while (*p >= '0' && *p <= '9') {
			v = v * 10 + (unsigned int)(*p - '0');
			p++;
			digits++;
			if (digits > 3 || v > 255) return 0;
		}
		parts[i] = v;
		n++;
	}
	if (*p != '\0' || n != 4) return 0;
	unsigned char *out = (unsigned char *)dst;
	out[0] = (unsigned char)parts[0];
	out[1] = (unsigned char)parts[1];
	out[2] = (unsigned char)parts[2];
	out[3] = (unsigned char)parts[3];
	return 1;
}

int
inet_aton(const char *cp, struct in_addr *addr)
{
	unsigned char bytes[4];
	if (inet_pton(AF_INET, cp, bytes) != 1) return 0;
	memcpy(&addr->s_addr, bytes, 4);
	return 1;
}

in_addr_t
inet_addr(const char *cp)
{
	struct in_addr a;
	if (inet_aton(cp, &a) == 0) return (in_addr_t)-1;
	return a.s_addr;
}

char *
inet_ntoa(struct in_addr in)
{
	static char buf[16];
	inet_ntop(AF_INET, &in.s_addr, buf, sizeof(buf));
	return buf;
}

unsigned int if_nametoindex(const char *ifname) { (void)ifname; return 0; }
char *if_indextoname(unsigned int ifindex, char *ifname) { (void)ifindex; (void)ifname; return (void *)0; }

int getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, size_t hostlen,
    char *serv, size_t servlen, int flags)
{
	(void)sa; (void)salen; (void)flags;
	if (host && hostlen) { host[0] = 0; }
	if (serv && servlen) { serv[0] = 0; }
	return -1;
}
