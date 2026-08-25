/* Copyright (c) 2026 Vihaan Nathan
 *
 * Umbrella header. Scope is deliberately v1: the object-model +
 * collection core real client code touches most, not the whole real
 * framework -- no CFRunLoop, CFBundle, CFStream/CFSocket, CFCalendar,
 * CFNotificationCenter, or CFPlugIn. CFDate/CFTimeZone/CFLocale/CFURL
 * were added in Foundation's phase (TODO.md) to back NSDate/NSTimeZone/
 * NSLocale/NSURL -- each genuinely minimal, see their own header
 * comments for what's cut. CFPropertyList (XML plist only, no binary
 * plist) was added in Phase 25 for SystemConfiguration/configd's real
 * config.defs wire format. See TODO.md's CoreFoundation phase entry for
 * the original full list.
 */
#ifndef __COREFOUNDATION_COREFOUNDATION_H__
#define __COREFOUNDATION_COREFOUNDATION_H__

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFSet.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFTimeZone.h>
#include <CoreFoundation/CFLocale.h>
#include <CoreFoundation/CFURL.h>
#include <CoreFoundation/CFPropertyList.h>

#endif /* __COREFOUNDATION_COREFOUNDATION_H__ */
