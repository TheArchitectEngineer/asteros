/* Copyright (c) 2026 Vihaan Nathan
 *
 * SecRandomCopyBytes on top of the kernel's real getentropy()
 * (src/xnu/bsd/dev/random/randomdev.c) -- the same entropy source
 * userland/libc's arc4random()/uuid.c already trust, not a userland PRNG
 * reimplementation. The kernel caps a single getentropy() call at 256
 * bytes ("Can't request more than 256 random bytes at once", randomdev.c)
 * and returns EINVAL above that, so a caller asking for more just loops.
 */
#include <Security/SecRandom.h>
#include <sys/random.h>
#include <stdint.h>

const SecRandomRef kSecRandomDefault = (SecRandomRef)0;

int SecRandomCopyBytes(SecRandomRef rnd, size_t count, void *bytes)
{
	(void)rnd; /* only kSecRandomDefault is supported, matching real Security.framework */
	if (bytes == NULL && count != 0) {
		return -1;
	}

	uint8_t *p = (uint8_t *)bytes;
	size_t remaining = count;
	while (remaining > 0) {
		size_t chunk = remaining > 256 ? 256 : remaining;
		if (getentropy(p, chunk) != 0) {
			return -1;
		}
		p += chunk;
		remaining -= chunk;
	}
	return 0;
}
