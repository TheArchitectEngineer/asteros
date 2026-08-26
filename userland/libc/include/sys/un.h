/* Stub -- no networking; see netdb.h. */
#ifndef _SYS_UN_H_
#define _SYS_UN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/socket.h>
#include <string.h>

struct sockaddr_un {
	unsigned char sun_len;
	sa_family_t   sun_family;
	char          sun_path[104];
};

/* actual length of an initialized sockaddr_un -- added for the X11
 * milestone (xtrans's Xtranssock.c), ground-truthed against
 * src/xnu/bsd/sys/un.h. */
#define SUN_LEN(su) \
	(sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))

#ifdef __cplusplus
}
#endif

#endif /* _SYS_UN_H_ */
