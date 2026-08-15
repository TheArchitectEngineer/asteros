/* Copyright (c) 2026 Vihaan Nathan
 *
 * Error code values copied verbatim from the vendored SDK's real
 * SecBase.h (build/SDKs/MacOSX10.15.sdk/.../Security.framework/Headers/
 * SecBase.h) -- kept numerically ABI-identical even though nothing here
 * links against real Apple Security.framework, so any ported client code
 * that switches on these numbers (rather than the symbolic names) still
 * behaves correctly.
 */
#ifndef __SECURITY_SECBASE_H__
#define __SECURITY_SECBASE_H__

#include <CoreFoundation/CoreFoundation.h>
#include <MacTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	errSecSuccess          = 0,      /* No error. */
	errSecUnimplemented    = -4,     /* Function or operation not implemented. */
	errSecParam            = -50,    /* One or more parameters passed to a function were not valid. */
	errSecAllocate         = -108,   /* Failed to allocate memory. */
	errSecNotAvailable     = -25291, /* No keychain is available. */
	errSecDuplicateItem    = -25299, /* The item already exists. */
	errSecItemNotFound     = -25300, /* The item cannot be found. */
	errSecInvalidItemRef   = -25304, /* The item reference is invalid. */
	errSecDecode           = -26275, /* Unable to decode the provided data. */
};

#ifdef __cplusplus
}
#endif

#endif /* __SECURITY_SECBASE_H__ */
