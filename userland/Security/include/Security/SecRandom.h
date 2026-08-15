/* Copyright (c) 2026 Vihaan Nathan
 *
 * SecRandomCopyBytes: real Apple API surface, backed by this kernel's
 * genuine entropy pool (src/xnu/bsd/dev/random/randomdev.c) via
 * getentropy() -- see SecRandom.c for the implementation.
 */
#ifndef __SECURITY_SECRANDOM_H__
#define __SECURITY_SECRANDOM_H__

#include <Security/SecBase.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef const struct __SecRandom *SecRandomRef;

/* Synonym for NULL -- SecRandomCopyBytes only recognizes this one value,
 * same as real Security.framework's documented behavior. Compile-time
 * constant (unlike SecItem.h's kSec* constants), so this is safely
 * `const`-qualified with no cross-image init-order concern. */
extern const SecRandomRef kSecRandomDefault;

int SecRandomCopyBytes(SecRandomRef rnd, size_t count, void *bytes);

#ifdef __cplusplus
}
#endif

#endif /* __SECURITY_SECRANDOM_H__ */
