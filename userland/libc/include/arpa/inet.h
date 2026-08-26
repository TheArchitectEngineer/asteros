/* Stub -- no networking; see netdb.h. */
#ifndef _ARPA_INET_H_
#define _ARPA_INET_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <netinet/in.h>

const char *inet_ntop(int af, const void *src, char *dst, unsigned int size);
int inet_pton(int af, const char *src, void *dst);
in_addr_t inet_addr(const char *cp);
int inet_aton(const char *cp, struct in_addr *addr);
char *inet_ntoa(struct in_addr in);

/* Real RFC 1706 NSAP-address-to-text formatter -- added for Phase 30
 * (real vendored libresolv's ns_print.c uses it only for human-readable
 * debug printing of NSAP DNS records, no header in this source tag
 * actually declares it). */
char *inet_nsap_ntoa(int binlen, const unsigned char *binary, char *ascii);

#ifdef __cplusplus
}
#endif

#endif /* _ARPA_INET_H_ */
