/* No DNS/hostname resolution in this kernel config (see docs/architecture.md)
 * -- the actual socket syscalls (socket/bind/connect/accept/send/recv/...)
 * are real, implemented in socket.c against the AF_UNIX transport the X11
 * milestone needs. This file only stubs the inet/DNS-name-resolution side,
 * which has no backing network stack to resolve against. */
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>

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

const char *inet_ntop(int af, const void *src, char *dst, unsigned int size) { (void)af; (void)src; (void)dst; (void)size; return (void *)0; }
int inet_pton(int af, const char *src, void *dst) { (void)af; (void)src; (void)dst; return -1; }
in_addr_t inet_addr(const char *cp) { (void)cp; return (in_addr_t)-1; }
int inet_aton(const char *cp, struct in_addr *addr) { (void)cp; (void)addr; return 0; }
char *inet_ntoa(struct in_addr in) { (void)in; static char buf[16] = "0.0.0.0"; return buf; }

int getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, size_t hostlen,
    char *serv, size_t servlen, int flags)
{
	(void)sa; (void)salen; (void)flags;
	if (host && hostlen) { host[0] = 0; }
	if (serv && servlen) { serv[0] = 0; }
	return -1;
}
