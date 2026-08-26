/* Real Darwin's own libc ships <sha1.h> with exactly this API (derived
 * from the public-domain BSD/CMU SHA-1 implementation) -- needed for
 * the X11 milestone (xorg-server's os/xsha1.c, --with-sha1=libc path,
 * used for MIT-MAGIC-COOKIE-1 auth cookie generation). FIPS 180-1. */
#ifndef _SHA1_H_
#define _SHA1_H_

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	uint32_t state[5];
	uint32_t count[2];
	unsigned char buffer[64];
} SHA1_CTX;

void SHA1Init(SHA1_CTX *context);
void SHA1Update(SHA1_CTX *context, const unsigned char *data, size_t len);
void SHA1Final(unsigned char digest[20], SHA1_CTX *context);

#ifdef __cplusplus
}
#endif

#endif /* _SHA1_H_ */
