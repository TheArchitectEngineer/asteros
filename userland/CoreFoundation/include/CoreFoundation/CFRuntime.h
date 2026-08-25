/* Copyright (c) 2026 Vihaan Nathan
 *
 * The object-model primitives every concrete CF type in this tree is
 * built on, exposed publicly here (matching real CF, which also ships
 * this exact header) for the same reason real Apple does: custom CFType
 * implementations outside CFBase.c itself. Phase 25's SCDynamicStorePrivate
 * (userland/configd/SCDynamicStoreInternal.h) is this project's first
 * consumer -- real Apple's own SCDynamicStorePrivate struct embeds a
 * CFRuntimeBase the exact same way.
 */
#ifndef __COREFOUNDATION_CFRUNTIME_H__
#define __COREFOUNDATION_CFRUNTIME_H__

#include <CoreFoundation/CFBase.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __CFRuntimeBase {
	void *isa;		/* NULL until Foundation calls _CFRuntimeBridgeClasses for this typeID; objc_object-layout-compatible */
	CFTypeID typeID;
	Boolean isConstant;	/* statically-allocated singletons (kCFBooleanTrue, kCFNull, ...): CFRetain/CFRelease are no-ops */
	volatile CFIndex retainCount;	/* touched only via __atomic_* builtins -- see pthread.c for the same convention */
} CFRuntimeBase;

typedef struct {
	const char *className;
	void (*finalize)(CFTypeRef cf);
	Boolean (*equal)(CFTypeRef cf1, CFTypeRef cf2);
	CFHashCode (*hash)(CFTypeRef cf);
	CFStringRef (*copyFormattingDesc)(CFTypeRef cf);
} CFRuntimeClass;

CF_EXPORT CFTypeID _CFRuntimeRegisterClass(const CFRuntimeClass *cls);
CF_EXPORT const CFRuntimeClass *_CFRuntimeGetClass(CFTypeID typeID);
CF_EXPORT CFTypeRef _CFRuntimeCreateInstance(CFAllocatorRef allocator, CFTypeID typeID, CFIndex extraBytes);
CF_EXPORT void _CFRuntimeInitStaticInstance(void *memory, CFTypeID typeID);
CF_EXPORT void _CFRuntimeBridgeClasses(CFTypeID typeID, void *isaClass);

#ifdef __cplusplus
}
#endif

#endif /* __COREFOUNDATION_CFRUNTIME_H__ */
