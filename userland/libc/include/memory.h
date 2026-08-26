/* Legacy pre-ANSI header some old BSD code (e.g. dst_api.c, Phase 30)
 * still #includes for memcpy/memset/memcmp -- real Darwin ships this
 * too, purely as a compatibility shim over string.h. */
#include <string.h>
