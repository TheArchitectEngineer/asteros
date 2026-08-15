/* Copyright (c) 2026 Vihaan Nathan
 *
 * v1 scope: only kSecClassGenericPassword is backed by real storage (an
 * in-memory, per-process keychain -- see SecItem.c's header comment for
 * why there's no on-disk persistence yet). kSecClassInternetPassword/
 * Certificate/Key/Identity are declared here for source compatibility
 * only; SecItemAdd/CopyMatching/Update/Delete return errSecParam for them.
 *
 * The kSecClass/kSecAttr/kSecReturn/kSecMatchLimit constants below are
 * declared `extern CFStringRef` (not `const`) -- these are real
 * heap-allocated CFStringRef instances built by a constructor in
 * SecItem.c, not compile-time literals (this CoreFoundation has no
 * CFSTR() builtin support). See
 * userland/Foundation/NSError.m's header comment: a `const`-qualified
 * version of this exact pattern broke cross-image reads, ground-truthed
 * live in QEMU.
 */
#ifndef __SECURITY_SECITEM_H__
#define __SECURITY_SECITEM_H__

#include <Security/SecBase.h>
#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif

extern CFStringRef kSecClass;
extern CFStringRef kSecClassGenericPassword;
extern CFStringRef kSecClassInternetPassword;
extern CFStringRef kSecClassCertificate;
extern CFStringRef kSecClassKey;
extern CFStringRef kSecClassIdentity;

extern CFStringRef kSecAttrService;
extern CFStringRef kSecAttrAccount;
extern CFStringRef kSecAttrLabel;

extern CFStringRef kSecValueData;

extern CFStringRef kSecReturnData;
extern CFStringRef kSecReturnAttributes;
extern CFStringRef kSecReturnRef;

extern CFStringRef kSecMatchLimit;
extern CFStringRef kSecMatchLimitOne;
extern CFStringRef kSecMatchLimitAll;

/* attributes must contain kSecClass (== kSecClassGenericPassword) and
 * kSecValueData; kSecAttrService/kSecAttrAccount/kSecAttrLabel are
 * optional item attributes. If result is non-NULL and attributes
 * contains kSecReturnData == kCFBooleanTrue, *result is set to a copy of
 * the stored CFDataRef (caller must CFRelease it). */
OSStatus SecItemAdd(CFDictionaryRef attributes, CFTypeRef *result);

/* query must contain kSecClass. kSecAttrService/Account/Label narrow the
 * search (omitted == wildcard). kSecMatchLimit defaults to matching a
 * single item (kSecMatchLimitOne semantics); pass kSecMatchLimitAll to
 * get every match back as a CFArrayRef. Per item, kSecReturnData yields a
 * CFDataRef, kSecReturnAttributes yields a CFDictionaryRef of the item's
 * attributes (plus kSecValueData if both are requested); if neither is
 * requested, *result is left NULL and the return code alone reports
 * whether a match exists. All returned objects are owned by the caller. */
OSStatus SecItemCopyMatching(CFDictionaryRef query, CFTypeRef *result);

/* Updates every item matching query. Only kSecValueData and
 * kSecAttrLabel are honored in attributesToUpdate in this v1 -- renaming
 * an item's service/account (which could collide with another item) is
 * not supported. */
OSStatus SecItemUpdate(CFDictionaryRef query, CFDictionaryRef attributesToUpdate);

/* Deletes every item matching query. */
OSStatus SecItemDelete(CFDictionaryRef query);

#ifdef __cplusplus
}
#endif

#endif /* __SECURITY_SECITEM_H__ */
