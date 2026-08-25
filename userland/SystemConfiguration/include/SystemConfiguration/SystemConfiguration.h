/* Copyright (c) 2026 Vihaan Nathan
 *
 * Umbrella header. v1 scope: SCDynamicStore only (the actual core the
 * rest of the real framework builds on) -- no SCPreferences (needs a
 * persistent plist-backed store this project doesn't have), no
 * SCNetworkConfiguration/SCNetworkReachability/SCNetworkConnection (need
 * richer live network state than Phase 24 currently provides -- loopback
 * only until its later milestones land a real NIC), no VPN/
 * CaptiveNetwork/DHCP/Bond/Bridge/VLAN (no such hardware or use case
 * here). See Phase 25's TODO.md entry for the full real-vs-cut list.
 *
 * SCDynamicStore.h/SCDynamicStoreKey.h/SCDynamicStoreCopySpecific.h
 * below are real, unmodified Apple public headers, copied verbatim from
 * this project's own vendored Catalina SDK (build/SDKs/MacOSX10.15.sdk),
 * the same vintage as the real configd/SystemConfiguration.fproj source
 * this phase vendors and adapts.
 */
#ifndef __SYSTEMCONFIGURATION_SYSTEMCONFIGURATION_H__
#define __SYSTEMCONFIGURATION_SYSTEMCONFIGURATION_H__

#include <os/availability.h>
#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Real error codes -- ground-truthed against the vendored real
 * SystemConfiguration.fproj/SystemConfiguration.h's own enum, the
 * generic + SCDynamicStore subsets only (SCPreferences/SCNetwork* codes
 * omitted, matching this file's own v1 scope above). */
enum {
	kSCStatusOK			= 0,
	kSCStatusFailed			= 1001,
	kSCStatusInvalidArgument	= 1002,
	kSCStatusAccessError		= 1003,
	kSCStatusNoKey			= 1004,
	kSCStatusKeyExists		= 1005,
	kSCStatusLocked			= 1006,
	kSCStatusNeedLock		= 1007,
	kSCStatusNoStoreSession		= 2001,
	kSCStatusNoStoreServer		= 2002,
	kSCStatusNotifierActive		= 2003,
};

int SCError(void);
const char *SCErrorString(int status);

#ifdef __cplusplus
}
#endif

#include <SystemConfiguration/SCDynamicStore.h>
#include <SystemConfiguration/SCDynamicStoreKey.h>
#include <SystemConfiguration/SCDynamicStoreCopySpecific.h>

#endif /* __SYSTEMCONFIGURATION_SYSTEMCONFIGURATION_H__ */
