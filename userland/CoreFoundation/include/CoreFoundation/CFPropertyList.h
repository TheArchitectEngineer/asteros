/* Copyright (c) 2026 Vihaan Nathan
 *
 * v1 scope: the classic XML-plist pair (CFPropertyListCreateXMLData/
 * CFPropertyListCreateFromXMLData) real CF has always shipped alongside
 * the newer CFErrorRef-based CFPropertyListCreateData/CreateWithData --
 * picked because this project has no CFErrorRef type yet, and because
 * this is the exact wire format Phase 25's config.defs (SystemConfiguration/
 * configd) needs: its xmlData/xmlDataOut MIG types are literally
 * serialized-XML property list bytes. Binary plist (bplist00) is not
 * implemented -- XML is the only format real callers of this exact API
 * pair ever produced anyway.
 *
 * Supported property types: CFDictionary, CFArray, CFString, CFNumber
 * (int and float forms), CFBoolean, CFData. CFDate and CFNull are not
 * representable here (matching classic CF's own plist DTD, which has no
 * <date> tag in this vintage and never allowed CFNull as a plist value).
 */
#ifndef __COREFOUNDATION_CFPROPERTYLIST_H__
#define __COREFOUNDATION_CFPROPERTYLIST_H__

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFString.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef CFTypeRef CFPropertyListRef;

typedef CFOptionFlags CFPropertyListMutabilityOptions;
enum {
	kCFPropertyListImmutable = 0,
	kCFPropertyListMutableContainers = 1,
	kCFPropertyListMutableContainersAndLeaves = 2
};

/* Serializes propertyList to XML plist bytes. Returns NULL if
 * propertyList (or something nested inside it) isn't a representable
 * property type. */
CF_EXPORT CFDataRef CFPropertyListCreateXMLData(CFAllocatorRef allocator, CFPropertyListRef propertyList);

/* Parses XML plist bytes back into a property list. On failure returns
 * NULL and, if errorString is non-NULL, hands back a CFStringRef the
 * caller owns (CFRelease it) describing what went wrong. mutabilityOption
 * controls whether containers (and, for the AndLeaves variant, CFString/
 * CFData leaves too) come back mutable or immutable. */
CF_EXPORT CFPropertyListRef CFPropertyListCreateFromXMLData(CFAllocatorRef allocator, CFDataRef xmlData, CFPropertyListMutabilityOptions mutabilityOption, CFStringRef *errorString);

/* Returns true if plist is built entirely out of the representable
 * property types (recursively) -- the same check CreateXMLData does
 * internally, exposed so callers can validate before serializing. */
CF_EXPORT Boolean CFPropertyListIsValid(CFPropertyListRef plist, CFStringRef xmlPropertyListVersion);

/* Not `const CFStringRef` (unlike kCFNull/kCFBooleanTrue in CFBase.h/
 * CFNumber.h) -- CFStringCreateWithCString() isn't a compile-time
 * constant expression, so this can't share their "address of a static
 * object populated later by a constructor" pattern; it's filled in by a
 * plain constructor-time assignment instead (see CFPropertyList.c). The
 * pointee is still effectively immutable -- CFStringRef itself already
 * points to const storage -- only the variable's own address binding
 * isn't compile-time-fixed. */
CF_EXPORT CFStringRef kCFPropertyListXMLFormatVersion1_0;

#ifdef __cplusplus
}
#endif

#endif /* __COREFOUNDATION_CFPROPERTYLIST_H__ */
