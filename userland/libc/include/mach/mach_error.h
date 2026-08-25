/* Real Darwin's mach_error_string() decodes a kern_return_t into a
 * human-readable string via a real error-system table. This project's
 * version just formats the numeric code -- it's used purely for
 * diagnostic logging (Phase 25's vendored SC_log/SC_trace call sites),
 * never parsed by anything, so losing the real symbolic names costs
 * nothing functionally. */
#ifndef _MACH_MACH_ERROR_H_
#define _MACH_MACH_ERROR_H_

#include <mach/kern_return.h>

#ifdef __cplusplus
extern "C" {
#endif

const char *mach_error_string(kern_return_t error_value);

#ifdef __cplusplus
}
#endif

#endif /* _MACH_MACH_ERROR_H_ */
