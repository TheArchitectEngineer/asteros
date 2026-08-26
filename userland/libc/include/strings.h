/* Real BSD <strings.h> -- distinct from <string.h>, needed for the X11
 * milestone (old BSD-style X.Org C source -- libxkbfile among others --
 * includes this directly for bcopy/bcmp/strcasecmp, not <string.h>).
 * strcasecmp/strncasecmp/bzero are already declared in string.h (this
 * project's existing, non-standard placement, kept as-is rather than
 * moved to avoid disturbing already-verified code); this header just
 * re-exposes them here too, matching real Darwin's own cross-declared
 * shape, and adds the BSD pair string.h doesn't have (bcopy/bcmp/ffs).
 */
#ifndef _STRINGS_H_
#define _STRINGS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <string.h>

void bcopy(const void *src, void *dst, size_t n);
int  bcmp(const void *a, const void *b, size_t n);
int  ffs(int i);
char *index(const char *s, int c);
char *rindex(const char *s, int c);

#ifdef __cplusplus
}
#endif

#endif /* _STRINGS_H_ */
