/* Stub -- no networking; see netdb.h. */
#ifndef _NETINET_IN_H_
#define _NETINET_IN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/socket.h>
#include <sys/_endian.h>
#include <stdint.h>

typedef uint32_t in_addr_t;
typedef uint16_t in_port_t;

struct in_addr {
	in_addr_t s_addr;
};

struct sockaddr_in {
	unsigned char  sin_len;
	sa_family_t    sin_family;
	in_port_t      sin_port;
	struct in_addr sin_addr;
	char           sin_zero[8];
};

struct in6_addr {
	unsigned char s6_addr[16];
};

struct sockaddr_in6 {
	unsigned char   sin6_len;
	sa_family_t     sin6_family;
	in_port_t       sin6_port;
	uint32_t        sin6_flowinfo;
	struct in6_addr sin6_addr;
	uint32_t        sin6_scope_id;
};

#define INADDR_ANY ((in_addr_t)0)

/* Real Darwin text-form address-buffer sizes -- added for Phase 30. */
#define INET_ADDRSTRLEN  16
#define INET6_ADDRSTRLEN 46

/* Real BSD/Darwin IPv4 address-class macros -- added for Phase 30
 * (real vendored libresolv's res_init.c uses these to detect a
 * classful local-network address). */
#define IN_CLASSA(i)		(((u_int32_t)(i) & 0x80000000) == 0)
#define IN_CLASSA_NET		0xff000000
#define IN_CLASSA_NSHIFT	24
#define IN_CLASSA_HOST		0x00ffffff
#define IN_CLASSA_MAX		128

#define IN_CLASSB(i)		(((u_int32_t)(i) & 0xc0000000) == 0x80000000)
#define IN_CLASSB_NET		0xffff0000
#define IN_CLASSB_NSHIFT	16
#define IN_CLASSB_HOST		0x0000ffff
#define IN_CLASSB_MAX		65536

#define IN_CLASSC(i)		(((u_int32_t)(i) & 0xe0000000) == 0xc0000000)
#define IN_CLASSC_NET		0xffffff00
#define IN_CLASSC_NSHIFT	8
#define IN_CLASSC_HOST		0x000000ff

#define IN_CLASSD(i)		(((u_int32_t)(i) & 0xf0000000) == 0xe0000000)
#define IN_MULTICAST(i)		IN_CLASSD(i)

#define IN_EXPERIMENTAL(i)	(((u_int32_t)(i) & 0xf0000000) == 0xf0000000)
#define IN_BADCLASS(i)		(((u_int32_t)(i) & 0xf0000000) == 0xf0000000)
#define IPPROTO_IP  0
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#define IPPROTO_IPV6 41

/* Real BSD ephemeral/local port ranges -- added for Phase 30. */
#define IPPORT_HIFIRSTAUTO	49152
#define IPPORT_HILASTAUTO	65535
#define IPPORT_RESERVED		1024

/* Real IP_* ancillary-data socket options -- added for Phase 30 (real
 * vendored libresolv's internal_recvfrom() requests IP_RECVIF to learn
 * which interface a UDP response arrived on). */
#ifndef IP_RECVIF
#define IP_RECVIF 20
#endif

/* Real IPV6_PKTINFO ancillary-data support -- added for Phase 30 (real
 * vendored libresolv's internal_recvfrom() needs it to learn which
 * interface a UDPv6 response arrived on). This project's netinet6/in6.h
 * already has the real, fuller IPv6 API surface including this same
 * struct/constant, but isn't included here to avoid redefining the
 * sockaddr_in6/in6_addr structs this file already declares above with
 * a simpler shape -- these two are pure additions, no name collision.
 */
#ifndef IPV6_PKTINFO
#define IPV6_PKTINFO 46
struct in6_pktinfo {
	struct in6_addr ipi6_addr;
	unsigned int    ipi6_ifindex;
};
#endif
#ifndef IPV6_MULTICAST_IF
#define IPV6_MULTICAST_IF 9
#endif

/* Real BSD multicast address-range constant -- added for Phase 30 (real
 * vendored libresolv's res_send.c uses it when a configured nameserver
 * happens to have a multicast address). */
#ifndef INADDR_MAX_LOCAL_GROUP
#define INADDR_MAX_LOCAL_GROUP (in_addr_t)0xe00000ffU /* 224.0.0.255 */
#endif

/* Real IN6_* address-test macros -- added for Phase 30. This project's
 * netinet6/in6.h already has these (and more), but isn't included here
 * to avoid redefining sockaddr_in6/in6_addr with this file's own
 * simpler shape (see the earlier IPV6_PKTINFO comment). */
#ifndef IN6_ARE_ADDR_EQUAL
#define IN6_ARE_ADDR_EQUAL(a, b) \
	(memcmp(&(a)->s6_addr[0], &(b)->s6_addr[0], sizeof(struct in6_addr)) == 0)
#endif
#ifndef IN6_IS_ADDR_UNSPECIFIED
#define IN6_IS_ADDR_UNSPECIFIED(a)      \
	((*(const uint32_t *)(const void *)(&(a)->s6_addr[0]) == 0) && \
	(*(const uint32_t *)(const void *)(&(a)->s6_addr[4]) == 0) && \
	(*(const uint32_t *)(const void *)(&(a)->s6_addr[8]) == 0) && \
	(*(const uint32_t *)(const void *)(&(a)->s6_addr[12]) == 0))
#endif
#ifndef IN6_IS_ADDR_MULTICAST
#define IN6_IS_ADDR_MULTICAST(a) ((a)->s6_addr[0] == 0xff)
#endif

/* IPPROTO_IP-level setsockopt options -- ground-truthed against
 * src/xnu/bsd/netinet/in.h. */
#define IP_HDRINCL       2   /* int; header is included with data */
#define IP_TTL           4   /* int; IP time to live */
#define IP_MULTICAST_IF  9   /* u_char; set/get IP multicast i/f */
#define IP_MULTICAST_TTL 10  /* u_char; set/get IP multicast ttl */

/* htons/ntohs/htonl/ntohl come from the #include <sys/_endian.h> above --
 * this used to redefine them here as static inline functions, which
 * collided (redefinition errors) whenever some other header in the same
 * translation unit pulled in sys/_endian.h's macro versions first (e.g.
 * busybox's libbb.h: pwd.h/grp.h -> machine/endian.h -> sys/_endian.h,
 * ahead of its own <netinet/in.h> include). sys/_endian.h is the real,
 * ground-truthed Darwin header for these; this file has no business
 * defining its own second copy. */

/* Real Apple SDK headers pull the arpa/inet.h declarations in
 * transitively from here -- busybox's include/libbb.h relies on exactly
 * that under __APPLE__ (only includes <netinet/in.h>, not
 * <arpa/inet.h>), so we mirror it rather than patch busybox. */
const char *inet_ntop(int af, const void *src, char *dst, unsigned int size);
int inet_pton(int af, const char *src, void *dst);
in_addr_t inet_addr(const char *cp);
int inet_aton(const char *cp, struct in_addr *addr);
char *inet_ntoa(struct in_addr in);

#ifdef __cplusplus
}
#endif

#endif /* _NETINET_IN_H_ */
