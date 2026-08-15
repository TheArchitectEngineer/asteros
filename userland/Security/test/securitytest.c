/* End-to-end proof of Security.framework's v1 surface: SecRandomCopyBytes
 * against real kernel entropy, and the full SecItemAdd/CopyMatching/
 * Update/Delete lifecycle against the in-memory kSecClassGenericPassword
 * keychain (see SecItem.c's header comment for what's out of scope).
 * Same pattern as userland/CoreFoundation/test/cftest.c -- a normal
 * dynamically-linked executable against libSecurity.dylib +
 * libCoreFoundation.dylib + libSystem.B.dylib.
 */
#include <Security/Security.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond, msg) \
	do { \
		if (!(cond)) { \
			printf("SECURITYTEST FAIL: %s\n", msg); \
			exit(1); \
		} \
	} while (0)

static void
test_random(void)
{
	uint8_t a[64];
	uint8_t b[64];
	memset(a, 0, sizeof(a));
	memset(b, 0, sizeof(b));

	CHECK(SecRandomCopyBytes(kSecRandomDefault, sizeof(a), a) == 0, "SecRandomCopyBytes a");
	CHECK(SecRandomCopyBytes(kSecRandomDefault, sizeof(b), b) == 0, "SecRandomCopyBytes b");

	Boolean allZeroA = true;
	for (size_t i = 0; i < sizeof(a); i++) {
		if (a[i] != 0) {
			allZeroA = false;
			break;
		}
	}
	CHECK(!allZeroA, "random buffer not all zero");
	CHECK(memcmp(a, b, sizeof(a)) != 0, "two independent random buffers differ");

	/* Exercises the >256-byte chunking loop (kernel getentropy() caps a
	 * single call at 256 bytes -- see SecRandom.c). */
	uint8_t big[600];
	memset(big, 0, sizeof(big));
	CHECK(SecRandomCopyBytes(kSecRandomDefault, sizeof(big), big) == 0, "SecRandomCopyBytes >256 bytes");
	Boolean allZeroBig = true;
	for (size_t i = 0; i < sizeof(big); i++) {
		if (big[i] != 0) {
			allZeroBig = false;
			break;
		}
	}
	CHECK(!allZeroBig, "large random buffer not all zero");

	printf("SECURITYTEST: SecRandomCopyBytes ok\n");
}

static CFDictionaryRef
makeAddDict(const char *service, const char *account, const char *dataStr)
{
	CFMutableDictionaryRef d = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(d, kSecClass, kSecClassGenericPassword);
	CFStringRef svc = CFStringCreateWithCString(kCFAllocatorDefault, service, kCFStringEncodingUTF8);
	CFStringRef acc = CFStringCreateWithCString(kCFAllocatorDefault, account, kCFStringEncodingUTF8);
	CFDictionarySetValue(d, kSecAttrService, svc);
	CFDictionarySetValue(d, kSecAttrAccount, acc);
	CFDataRef data = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)dataStr, (CFIndex)strlen(dataStr));
	CFDictionarySetValue(d, kSecValueData, data);
	CFRelease(svc);
	CFRelease(acc);
	CFRelease(data);
	return d;
}

static CFDictionaryRef
makeQueryDict(const char *service, const char *account, Boolean wantData)
{
	CFMutableDictionaryRef d = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(d, kSecClass, kSecClassGenericPassword);
	CFStringRef svc = CFStringCreateWithCString(kCFAllocatorDefault, service, kCFStringEncodingUTF8);
	CFStringRef acc = CFStringCreateWithCString(kCFAllocatorDefault, account, kCFStringEncodingUTF8);
	CFDictionarySetValue(d, kSecAttrService, svc);
	CFDictionarySetValue(d, kSecAttrAccount, acc);
	if (wantData) {
		CFDictionarySetValue(d, kSecReturnData, kCFBooleanTrue);
	}
	CFRelease(svc);
	CFRelease(acc);
	return d;
}

static void
test_keychain_lifecycle(void)
{
	CFDictionaryRef add = makeAddDict("asteros.test", "alice", "s3cr3t");
	OSStatus st = SecItemAdd(add, NULL);
	CHECK(st == errSecSuccess, "SecItemAdd success");
	CFRelease(add);

	/* duplicate service+account must be rejected */
	CFDictionaryRef dup = makeAddDict("asteros.test", "alice", "different");
	st = SecItemAdd(dup, NULL);
	CHECK(st == errSecDuplicateItem, "SecItemAdd duplicate rejected");
	CFRelease(dup);

	/* copy it back out */
	CFDictionaryRef q = makeQueryDict("asteros.test", "alice", true);
	CFTypeRef result = NULL;
	st = SecItemCopyMatching(q, &result);
	CHECK(st == errSecSuccess, "SecItemCopyMatching found");
	CHECK(result != NULL, "SecItemCopyMatching returned data");
	CFDataRef data = (CFDataRef)result;
	CHECK(CFDataGetLength(data) == 6, "retrieved data length");
	CHECK(memcmp(CFDataGetBytePtr(data), "s3cr3t", 6) == 0, "retrieved data contents");
	CFRelease(result);
	CFRelease(q);

	/* a query for a different account must miss */
	CFDictionaryRef missQ = makeQueryDict("asteros.test", "bob", false);
	CFTypeRef missResult = (CFTypeRef)1; /* poison value -- must come back NULL */
	st = SecItemCopyMatching(missQ, &missResult);
	CHECK(st == errSecItemNotFound, "SecItemCopyMatching miss");
	CHECK(missResult == NULL, "SecItemCopyMatching miss leaves *result NULL");
	CFRelease(missQ);

	/* update the item's data */
	CFMutableDictionaryRef updQuery = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(updQuery, kSecClass, kSecClassGenericPassword);
	CFStringRef svc = CFStringCreateWithCString(kCFAllocatorDefault, "asteros.test", kCFStringEncodingUTF8);
	CFStringRef acc = CFStringCreateWithCString(kCFAllocatorDefault, "alice", kCFStringEncodingUTF8);
	CFDictionarySetValue(updQuery, kSecAttrService, svc);
	CFDictionarySetValue(updQuery, kSecAttrAccount, acc);
	CFRelease(svc);
	CFRelease(acc);

	CFMutableDictionaryRef upd = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDataRef newData = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)"updated", 7);
	CFDictionarySetValue(upd, kSecValueData, newData);
	CFRelease(newData);

	st = SecItemUpdate(updQuery, upd);
	CHECK(st == errSecSuccess, "SecItemUpdate success");
	CFRelease(upd);

	CFDictionaryRef q2 = makeQueryDict("asteros.test", "alice", true);
	CFTypeRef result2 = NULL;
	st = SecItemCopyMatching(q2, &result2);
	CHECK(st == errSecSuccess, "SecItemCopyMatching after update found");
	CFDataRef data2 = (CFDataRef)result2;
	CHECK(CFDataGetLength(data2) == 7, "updated data length");
	CHECK(memcmp(CFDataGetBytePtr(data2), "updated", 7) == 0, "updated data contents");
	CFRelease(result2);
	CFRelease(q2);

	/* delete it, then confirm it's gone */
	st = SecItemDelete(updQuery);
	CHECK(st == errSecSuccess, "SecItemDelete success");
	CFRelease(updQuery);

	CFDictionaryRef q3 = makeQueryDict("asteros.test", "alice", false);
	st = SecItemCopyMatching(q3, NULL);
	CHECK(st == errSecItemNotFound, "item gone after delete");
	CFRelease(q3);

	/* deleting again must report not-found, not crash */
	CFDictionaryRef q4 = makeQueryDict("asteros.test", "alice", false);
	st = SecItemDelete(q4);
	CHECK(st == errSecItemNotFound, "double delete reports not found");
	CFRelease(q4);

	printf("SECURITYTEST: SecItem lifecycle ok\n");
}

static void
test_keychain_match_all(void)
{
	CFDictionaryRef a1 = makeAddDict("asteros.multi", "one", "aaa");
	CFDictionaryRef a2 = makeAddDict("asteros.multi", "two", "bbb");
	CHECK(SecItemAdd(a1, NULL) == errSecSuccess, "multi add 1");
	CHECK(SecItemAdd(a2, NULL) == errSecSuccess, "multi add 2");
	CFRelease(a1);
	CFRelease(a2);

	CFMutableDictionaryRef q = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(q, kSecClass, kSecClassGenericPassword);
	CFStringRef svc = CFStringCreateWithCString(kCFAllocatorDefault, "asteros.multi", kCFStringEncodingUTF8);
	CFDictionarySetValue(q, kSecAttrService, svc);
	CFRelease(svc);
	CFDictionarySetValue(q, kSecMatchLimit, kSecMatchLimitAll);
	CFDictionarySetValue(q, kSecReturnData, kCFBooleanTrue);

	CFTypeRef result = NULL;
	OSStatus st = SecItemCopyMatching(q, &result);
	CHECK(st == errSecSuccess, "match-all success");
	CHECK(result != NULL, "match-all returned array");
	CFArrayRef arr = (CFArrayRef)result;
	CHECK(CFArrayGetCount(arr) == 2, "match-all count == 2");
	CFRelease(result);
	CFRelease(q);

	/* clean up so later runs of this process (if any) don't see stale items */
	CFMutableDictionaryRef delQ = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(delQ, kSecClass, kSecClassGenericPassword);
	CFStringRef svc2 = CFStringCreateWithCString(kCFAllocatorDefault, "asteros.multi", kCFStringEncodingUTF8);
	CFDictionarySetValue(delQ, kSecAttrService, svc2);
	CFRelease(svc2);
	CHECK(SecItemDelete(delQ) == errSecSuccess, "match-all cleanup delete");
	CFRelease(delQ);

	printf("SECURITYTEST: SecItem kSecMatchLimitAll ok\n");
}

static void
test_keychain_param_errors(void)
{
	/* unsupported class */
	CFMutableDictionaryRef bad = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(bad, kSecClass, kSecClassInternetPassword);
	CHECK(SecItemAdd(bad, NULL) == errSecParam, "unsupported class rejected");
	CFRelease(bad);

	/* missing kSecClass entirely */
	CFDictionaryRef empty = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CHECK(SecItemAdd(empty, NULL) == errSecParam, "missing class rejected");
	CFRelease(empty);

	printf("SECURITYTEST: SecItem param validation ok\n");
}

int
main(void)
{
	printf("SECURITYTEST: starting\n");
	test_random();
	test_keychain_lifecycle();
	test_keychain_match_all();
	test_keychain_param_errors();
	printf("SECURITYTEST PASS\n");
	return 0;
}
