/* Stub -- no networking; see netdb.h. */
#ifndef _SYS_SOCKET_H_
#define _SYS_SOCKET_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/uio.h> /* struct iovec, for struct msghdr */

typedef unsigned int socklen_t;
/* Real xnu's sa_family_t is __uint8_t (bsd/sys/_types/_sa_family_t.h), not
 * a 16-bit type -- ground-truthed against src/xnu/bsd/sys/socket.h. Getting
 * this wrong silently shifts every sockaddr_in/sockaddr_un field after
 * sa_family by a byte on the wire (2-byte alignment padding this project's
 * old `unsigned short` version would need, that the real 1-byte kernel
 * struct doesn't), which nothing had caught yet since no AF_INET or AF_UNIX
 * sockaddr had actually round-tripped through a syscall before the Phase 24
 * networking milestone. */
typedef unsigned char sa_family_t;

struct sockaddr {
	unsigned char sa_len;
	sa_family_t   sa_family;
	char          sa_data[14];
};

struct sockaddr_storage {
	unsigned char ss_len;
	sa_family_t   ss_family;
	char          ss_pad[126];
};

#define AF_UNSPEC 0
#define AF_UNIX   1
#define AF_LOCAL  AF_UNIX
#define AF_INET   2
#define AF_INET6  30
#define SOCK_STREAM    1
#define SOCK_DGRAM     2
#define SOCK_RAW       3
#define SOCK_RDM       4
#define SOCK_SEQPACKET 5

#define SHUT_RD   0
#define SHUT_WR   1
#define SHUT_RDWR 2

#define SOL_SOCKET    0xffff
#define SO_REUSEADDR  0x0004
#define SO_BROADCAST  0x0020
#define SO_KEEPALIVE  0x0008
#define SO_SNDBUF     0x1001
#define SO_RCVBUF     0x1002
#define SO_ERROR      0x1007

int socket(int domain, int type, int protocol);
int bind(int s, const struct sockaddr *addr, socklen_t len);
int connect(int s, const struct sockaddr *addr, socklen_t len);
int listen(int s, int backlog);
int accept(int s, struct sockaddr *addr, socklen_t *len);
int getsockname(int s, struct sockaddr *addr, socklen_t *len);
int getpeername(int s, struct sockaddr *addr, socklen_t *len);
ssize_t send(int s, const void *buf, size_t len, int flags);
ssize_t sendto(int s, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen);
ssize_t recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);
ssize_t recv(int s, void *buf, size_t len, int flags);
int setsockopt(int s, int level, int optname, const void *optval, socklen_t optlen);
int getsockopt(int s, int level, int optname, void *optval, socklen_t *optlen);
int shutdown(int s, int how);

/* Real BSD/Darwin sendmsg/recvmsg + ancillary (control) message support
 * -- added for Phase 30 (real vendored libresolv's internal_recvfrom()
 * uses IP_RECVIF/IPV6_PKTINFO control messages to learn which interface
 * a UDP response arrived on). Struct layouts and CMSG_* macros are
 * standard, unchanging BSD socket API -- ground-truthed against real
 * Darwin's actual shapes. */
#ifndef MSG_CTRUNC
#define MSG_OOB		0x1
#define MSG_PEEK	0x2
#define MSG_DONTROUTE	0x4
#define MSG_EOR		0x8
#define MSG_TRUNC	0x10
#define MSG_CTRUNC	0x20
#define MSG_WAITALL	0x40
#define MSG_DONTWAIT	0x80
#endif

struct msghdr {
	void		*msg_name;
	socklen_t	msg_namelen;
	struct iovec	*msg_iov;
	int		msg_iovlen;
	void		*msg_control;
	socklen_t	msg_controllen;
	int		msg_flags;
};

struct cmsghdr {
	socklen_t	cmsg_len;
	int		cmsg_level;
	int		cmsg_type;
};

#define __CMSG_ALIGN(n) (((n) + sizeof(long) - 1) & ~(sizeof(long) - 1))

#define CMSG_FIRSTHDR(mhdr) \
	((size_t)(mhdr)->msg_controllen >= sizeof(struct cmsghdr) ? \
	 (struct cmsghdr *)(mhdr)->msg_control : (struct cmsghdr *)0)

#define CMSG_DATA(cmsg) ((unsigned char *)(cmsg) + __CMSG_ALIGN(sizeof(struct cmsghdr)))

#define CMSG_NXTHDR(mhdr, cmsg) \
	(((unsigned char *)(cmsg) + __CMSG_ALIGN((cmsg)->cmsg_len) \
	    + __CMSG_ALIGN(sizeof(struct cmsghdr)) > \
	  (unsigned char *)(mhdr)->msg_control + (mhdr)->msg_controllen) ? \
	 (struct cmsghdr *)0 : \
	 (struct cmsghdr *)((unsigned char *)(cmsg) + __CMSG_ALIGN((cmsg)->cmsg_len)))

#define CMSG_SPACE(l) (__CMSG_ALIGN(sizeof(struct cmsghdr)) + __CMSG_ALIGN(l))
#define CMSG_LEN(l)   (__CMSG_ALIGN(sizeof(struct cmsghdr)) + (l))

ssize_t sendmsg(int s, const struct msghdr *msg, int flags);
ssize_t recvmsg(int s, struct msghdr *msg, int flags);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_SOCKET_H_ */
