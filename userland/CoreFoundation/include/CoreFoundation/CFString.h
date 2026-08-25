/* Copyright (c) 2026 Vihaan Nathan
 *
 * v1 simplification: strings are stored as UTF-8 internally rather than
 * real CF's UTF-16 UniChar buffers. CFStringGetLength()/
 * CFStringGetCharacterAtIndex() still hand back UTF-16 code units (they
 * decode UTF-8 on the fly), so correctly-written client code sees the
 * documented behavior -- the only real gap is codepoints outside the
 * BMP, which would need surrogate pairs this decoder doesn't produce.
 * Documented, not silent: see TODO.md's CoreFoundation phase writeup.
 */
#ifndef __COREFOUNDATION_CFSTRING_H__
#define __COREFOUNDATION_CFSTRING_H__

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFArray.h>
#include <stdarg.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned short UniChar;
typedef uint32_t CFStringEncoding;

#define kCFStringEncodingInvalidId ((CFStringEncoding)0xffffffffU)
enum {
	kCFStringEncodingMacRoman = 0,
	kCFStringEncodingASCII = 0x0600,
	kCFStringEncodingUTF8 = 0x08000100,
	kCFStringEncodingUnicode = 0x0100,
	kCFStringEncodingUTF16 = 0x0100
};

typedef CFOptionFlags CFStringCompareFlags;
enum {
	kCFCompareCaseInsensitive = 1,
	kCFCompareBackwards = 4,
	kCFCompareAnchored = 8
};

CF_EXPORT CFTypeID CFStringGetTypeID(void);

/* Real CF's CFSTR(cStr) compiles a string literal into a genuine
 * compile-time constant (a special linker section clang/ld64 handle
 * specially on real Darwin) -- no such mechanism exists here, so this
 * creates a real (heap-allocated, UTF-8) CFStringRef at the point of use
 * instead. Correct but not free: unlike real CFSTR, this allocates on
 * every evaluation, so a CFSTR(...) used as, e.g., a dictionary key
 * constant inside a hot loop pays a real allocation each time -- fine
 * for how this project's vendored Phase 25 (SystemConfiguration/configd)
 * source actually uses it (mostly one-shot key/format-string
 * construction), not a general-purpose replacement for a real interned
 * string table. */
#define CFSTR(cStr) CFStringCreateWithCString(kCFAllocatorDefault, (cStr), kCFStringEncodingUTF8)

/* ---- creation ---- */
CF_EXPORT CFStringRef CFStringCreateWithCString(CFAllocatorRef alloc, const char *cStr, CFStringEncoding encoding);
CF_EXPORT CFStringRef CFStringCreateWithBytes(CFAllocatorRef alloc, const UInt8 *bytes, CFIndex numBytes, CFStringEncoding encoding, Boolean isExternalRepresentation);
CF_EXPORT CFStringRef CFStringCreateCopy(CFAllocatorRef alloc, CFStringRef theString);
CF_EXPORT CFStringRef CFStringCreateWithFormat(CFAllocatorRef alloc, CFTypeRef formatOptions, CFStringRef format, ...);
CF_EXPORT CFStringRef CFStringCreateWithFormatAndArguments(CFAllocatorRef alloc, CFTypeRef formatOptions, CFStringRef format, va_list arguments);

CF_EXPORT CFMutableStringRef CFStringCreateMutable(CFAllocatorRef alloc, CFIndex maxLength);
CF_EXPORT CFMutableStringRef CFStringCreateMutableCopy(CFAllocatorRef alloc, CFIndex maxLength, CFStringRef theString);

/* ---- inspection ---- */
CF_EXPORT CFIndex CFStringGetLength(CFStringRef theString);
CF_EXPORT UniChar CFStringGetCharacterAtIndex(CFStringRef theString, CFIndex idx);
CF_EXPORT void CFStringGetCharacters(CFStringRef theString, CFRange range, UniChar *buffer);
CF_EXPORT Boolean CFStringGetCString(CFStringRef theString, char *buffer, CFIndex bufferSize, CFStringEncoding encoding);
CF_EXPORT CFIndex CFStringGetBytes(CFStringRef theString, CFRange range, CFStringEncoding encoding, UInt8 lossByte, Boolean isExternalRepresentation, UInt8 *buffer, CFIndex maxBufLen, CFIndex *usedBufLen);
CF_EXPORT const char *CFStringGetCStringPtr(CFStringRef theString, CFStringEncoding encoding);
CF_EXPORT CFIndex CFStringGetLength(CFStringRef theString);
CF_EXPORT CFIndex CFStringGetMaximumSizeForEncoding(CFIndex length, CFStringEncoding encoding);

CF_EXPORT CFComparisonResult CFStringCompare(CFStringRef theString1, CFStringRef theString2, CFStringCompareFlags compareOptions);
CF_EXPORT Boolean CFStringHasPrefix(CFStringRef theString, CFStringRef prefix);
CF_EXPORT Boolean CFStringHasSuffix(CFStringRef theString, CFStringRef suffix);
/* Real CFStringFind returns a CFRange by value (.location == kCFNotFound
 * if not found) -- this project's own original 4-arg/Boolean-return
 * shape (kept, unchanged, for its existing callers -- userland/
 * Foundation/NSString.m) is CFStringFindWithOptions instead, the closer
 * real name for that shape. Vendoring Phase 25's real config code (which
 * calls the real 3-arg CFStringFind directly) is what surfaced this
 * naming mismatch. */
CF_EXPORT CFRange CFStringFind(CFStringRef theString, CFStringRef stringToFind, CFStringCompareFlags compareOptions);
CF_EXPORT Boolean CFStringFindWithOptions(CFStringRef theString, CFStringRef stringToFind, CFStringCompareFlags compareOptions, CFRange *result);
CF_EXPORT CFStringRef CFStringCreateWithSubstring(CFAllocatorRef alloc, CFStringRef str, CFRange range);

/* ---- mutation ---- */
CF_EXPORT void CFStringAppend(CFMutableStringRef theString, CFStringRef appendedString);
CF_EXPORT void CFStringAppendCString(CFMutableStringRef theString, const char *cStr, CFStringEncoding encoding);
CF_EXPORT void CFStringAppendFormat(CFMutableStringRef theString, CFTypeRef formatOptions, CFStringRef format, ...);
CF_EXPORT void CFStringInsert(CFMutableStringRef str, CFIndex idx, CFStringRef insertedStr);
CF_EXPORT void CFStringDelete(CFMutableStringRef theString, CFRange range);
CF_EXPORT void CFStringPad(CFMutableStringRef theString, CFStringRef padString, CFIndex length, CFIndex indexIntoPad);

CF_EXPORT CFArrayRef CFStringCreateArrayBySeparatingStrings(CFAllocatorRef alloc, CFStringRef theString, CFStringRef separatorString);
CF_EXPORT CFStringRef CFStringCreateByCombiningStrings(CFAllocatorRef alloc, CFArrayRef theArray, CFStringRef separatorString);

#ifdef __cplusplus
}
#endif

#endif /* __COREFOUNDATION_CFSTRING_H__ */
