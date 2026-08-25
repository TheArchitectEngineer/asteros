/* Copyright (c) 2026 Vihaan Nathan
 *
 * Trimmed replacement for real Apple SCPrivate.h (configd-963.270.3) --
 * that file is ~1700 lines covering keychain access, XPC helpers, crash
 * reporting, and a real os_log-integrated SC_log/SC_trace macro system.
 * This is the small real subset both configd.tproj and this project's
 * SystemConfiguration client library actually need: the serialization
 * helpers (real function signatures, real behavior, adapted in
 * SCDPrivate.c to XML plist instead of binary -- see that file's own
 * comment) and isA_CF* (real, unmodified, in SCValidation.h already).
 */
#ifndef _SCPRIVATE_H
#define _SCPRIVATE_H

#include <stdio.h>
#include <sys/cdefs.h>
#include <sys/syslog.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCDynamicStore.h>

/* SC_log/SC_trace: real SCPrivate.h's versions do a lot more (os_log_type
 * mapping, %@ CFString-aware formatting via __SC_Log) -- this project
 * has no os_log and no %@ format specifier support, so these are plain
 * printf-based, same substitution configd.h's own copy uses server-side
 * (guarded with #ifndef there so whichever of the two gets included
 * first -- this header via SCDynamicStoreInternal.h, or configd.h
 * directly -- wins without a macro-redefinition warning). */
#ifndef SC_log
#define SC_log(__level, __format, ...) printf("[SC %d] " __format "\n", (__level), ##__VA_ARGS__)
#endif
#ifndef SC_trace
#define SC_trace(__string, ...) os_log_debug(SC_LOG_HANDLE, __string, ##__VA_ARGS__)
#endif

__BEGIN_DECLS

/* Not part of the real public SCDynamicStore.h (declared in real
 * SCPrivate.h instead, despite the un-underscored name) -- called from
 * both SCDOpen.c's __SCDynamicStoreDeallocate() and directly by client
 * code cancelling its own notification. */
Boolean SCDynamicStoreNotifyCancel(SCDynamicStoreRef store);

/* Real Apple declares this in SCDynamicStorePrivate.h (an SPI header,
 * not part of the public SDK -- grepped the real vendored SDK headers
 * to confirm), not the public SCDynamicStore.h -- same treatment as
 * SCDynamicStoreNotifyCancel just above. */
Boolean SCDynamicStoreNotifyFileDescriptor(SCDynamicStoreRef store, int32_t identifier, int *fd);

/* Per-thread last-error state (SCD.c) -- SCError()/SCErrorString() are
 * public API (SystemConfiguration.h); _SCErrorSet() is the internal
 * setter real vendored source calls throughout. */
void _SCErrorSet(int error);

Boolean _SCSerialize(CFPropertyListRef obj, CFDataRef *xml, void **dataRef, CFIndex *dataLen);
Boolean _SCUnserialize(CFPropertyListRef *obj, CFDataRef xml, void *dataRef, CFIndex dataLen);
Boolean _SCSerializeString(CFStringRef str, CFDataRef *data, void **dataRef, CFIndex *dataLen);
Boolean _SCUnserializeString(CFStringRef *str, CFDataRef utf8, void *dataRef, CFIndex dataLen);
Boolean _SCSerializeData(CFDataRef data, void **dataRef, CFIndex *dataLen);
Boolean _SCUnserializeData(CFDataRef *data, void *dataRef, CFIndex dataLen);

/* Thin wrapper over CFStringGetCString -- real signature/behavior
 * (returns buf on success, NULL on failure), used by pattern.c's
 * regexec()-based key matching. */
char *_SC_cfstring_to_cstring(CFStringRef cfstring, char *buf, CFIndex bufLen, CFStringEncoding encoding);

__END_DECLS

#endif /* _SCPRIVATE_H */
