/* Real ifreq/ifconf + interface flags, ground-truthed against
 * src/xnu/bsd/net/if.h's non-PRIVATE, non-KERNEL_PRIVATE surface (this is
 * a userland header; those two macros are never defined here, so the
 * ifr_ifru union below only needs to match what the real kernel struct
 * looks like under that same condition). The `ifru_pad` member pins the
 * union to the real kernel's 16-byte union size regardless of which other
 * members this file chooses to model, so a GET ioctl (e.g. SIOCGIFFLAGS)
 * writing back sizeof(struct ifreq) bytes can never overrun this buffer
 * even if a future caller needs a union member not listed here. */
#ifndef _NET_IF_H_
#define _NET_IF_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/sockio.h>

#define IFNAMSIZ 16

#define IFF_UP          0x1             /* interface is up */
#define IFF_BROADCAST   0x2             /* broadcast address valid */
#define IFF_DEBUG       0x4             /* turn on debugging */
#define IFF_LOOPBACK    0x8             /* is a loopback net */
#define IFF_POINTOPOINT 0x10            /* interface is point-to-point link */
#define IFF_NOTRAILERS  0x20            /* obsolete: avoid use of trailers */
#define IFF_RUNNING     0x40            /* resources allocated */
#define IFF_NOARP       0x80            /* no address resolution protocol */
#define IFF_PROMISC     0x100           /* receive all packets */
#define IFF_ALLMULTI    0x200           /* receive all multicast packets */
#define IFF_OACTIVE     0x400           /* transmission in progress */
#define IFF_SIMPLEX     0x800           /* can't hear own transmissions */
#define IFF_LINK0       0x1000          /* per link layer defined bit */
#define IFF_LINK1       0x2000          /* per link layer defined bit */
#define IFF_LINK2       0x4000          /* per link layer defined bit */
#define IFF_ALTPHYS     IFF_LINK2       /* use alternate physical connection */
#define IFF_MULTICAST   0x8000          /* supports multicast */
/* Linux-only flag busybox's interface.c flag-name table references; no BSD
 * driver ever sets this bit (real bonding is a distinct pseudo-interface
 * here, not a per-member flag), so an unused high bit is a safe, always-off
 * placeholder rather than colliding with a real IFF_* value above. */
#define IFF_SLAVE       0x00010000
#define IFF_MASTER      0x00020000

/* Linux-only struct ifconfig's struct interface embeds unconditionally
 * (SIOCGIFMAP's own payload on Linux) -- no BSD counterpart exists, so
 * this is layout-only, matching whatever busybox happens to memcpy/zero
 * into it; SIOCGIFMAP itself (below) always fails against this kernel, so
 * these fields are never populated by anything real. Declared ahead of
 * struct ifreq so ifr_ifru's union below can embed one directly, the same
 * way Linux's own ifreq does.
 *
 * mem_start/mem_end are `unsigned int` here, NOT Linux's `unsigned long`:
 * every _IOW/_IOWR SIOC* macro bakes sizeof(struct ifreq) into the
 * ioctl command's own numeric value (see <sys/ioccom.h>), so ifr_ifru's
 * union has to stay within the *kernel's* 16-byte union size (ground-
 * truthed against src/xnu/bsd/net/if.h's non-PRIVATE ifr_ifru) or every
 * SIOC*(..., struct ifreq) command this file defines silently encodes a
 * different 32-bit value than the kernel's own same-named constant --
 * caught live: with `unsigned long` fields this struct was 24 bytes,
 * inflating sizeof(struct ifreq) from the real 32 to 40 and making
 * SIOCSIFADDR itself no longer match any case in ifioctllocked()'s
 * switch, silently falling through to EOPNOTSUPP instead of failing to
 * compile or erroring obviously. */
struct ifmap {
	unsigned int mem_start;
	unsigned int mem_end;
	unsigned short base_addr;
	unsigned char irq;
	unsigned char dma;
	unsigned char port;
};

struct ifreq {
	char ifr_name[IFNAMSIZ];        /* if name, e.g. "en0" */
	union {
		struct  sockaddr ifru_addr;
		struct  sockaddr ifru_dstaddr;
		struct  sockaddr ifru_broadaddr;
		short   ifru_flags;
		int     ifru_metric;
		int     ifru_mtu;
		int     ifru_phys;
		int     ifru_media;
		int     ifru_intval;
		char   *ifru_data;
		int     ifru_cap[2];
		struct  ifmap ifru_map; /* Linux-only, no BSD ioctl ever fills it */
		char    ifru_pad[16];   /* floor: at least the real kernel's union size */
	} ifr_ifru;
#define ifr_addr        ifr_ifru.ifru_addr
#define ifr_dstaddr     ifr_ifru.ifru_dstaddr
#define ifr_broadaddr   ifr_ifru.ifru_broadaddr
#define ifr_flags       ifr_ifru.ifru_flags
#define ifr_metric      ifr_ifru.ifru_metric
#define ifr_mtu         ifr_ifru.ifru_mtu
#define ifr_phys        ifr_ifru.ifru_phys
#define ifr_media       ifr_ifru.ifru_media
#define ifr_intval      ifr_ifru.ifru_intval
#define ifr_data        ifr_ifru.ifru_data
/* Linux-only ifreq union aliases busybox's interface.c/ifconfig.c use.
 * BSD has no dedicated ifr_netmask field -- SIOCGIFNETMASK/SIOCSIFNETMASK
 * both reuse ifr_addr's struct-sockaddr slot for the mask (ground-truthed
 * against src/xnu/bsd/netinet/in.c's in_control(), SIOCGIFNETMASK/
 * SIOCSIFNETMASK cases), so this aliases to the same union member rather
 * than adding a new one. ifr_hwaddr has no real BSD backing at all --
 * this kernel has no SIOCGIFHWADDR (see SIOCGIFHWADDR's own comment
 * below), so this alias exists purely so busybox's struct layout compiles;
 * the ioctl that would populate it always fails safely (ENOTTY). */
#define ifr_netmask     ifr_ifru.ifru_addr
#define ifr_hwaddr      ifr_ifru.ifru_addr
#define ifr_map         ifr_ifru.ifru_map
};

struct ifconf {
	int     ifc_len;                /* size of associated buffer */
	union {
		char            *ifcu_buf;
		struct ifreq    *ifcu_req;
	} ifc_ifcu;
#define ifc_buf ifc_ifcu.ifcu_buf
#define ifc_req ifc_ifcu.ifcu_req
};

/* Real xnu's struct {if,in_}aliasreq -- ground-truthed against
 * src/xnu/bsd/netinet/in_var.h's in_aliasreq, the one SIOCAIFADDR actually
 * copies in for AF_INET. Not used by this project's ifconfig/networktest
 * yet (they use the simpler SIOCSIFADDR/SIOCSIFNETMASK/SIOCSIFFLAGS
 * ioctls, which busybox's own ifconfig applet also uses), but declared
 * here since <sys/sockio.h> references it in SIOCAIFADDR's own comment. */
struct ifaliasreq {
	char            ifra_name[IFNAMSIZ];
	struct  sockaddr ifra_addr;
	struct  sockaddr ifra_broadaddr;
	struct  sockaddr ifra_mask;
};

struct if_nameindex {
	unsigned int if_index;
	char *if_name;
};

/* Real Darwin implements these via a PF_ROUTE/getifaddrs()-style interface
 * enumeration this project doesn't have yet (no routing socket, no
 * SIOCGIFINDEX -- grepped, genuinely absent from src/xnu unlike on Linux).
 * if_nametoindex honestly reports "no such interface" (0) rather than
 * pretending to resolve one; busybox's ping -I already handles that
 * return by falling back to treating its argument as an address instead
 * of an interface name. */
unsigned int if_nametoindex(const char *ifname);
char *if_indextoname(unsigned int ifindex, char *ifname);

#ifdef __cplusplus
}
#endif

#endif /* _NET_IF_H_ */
