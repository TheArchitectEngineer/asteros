/* Copyright (c) 2026 Vihaan Nathan
 *
 * v1-scoped keychain: kSecClassGenericPassword only, held in an
 * in-memory, per-process linked list guarded by a real pthread_mutex_t
 * (Phase 16) -- no on-disk persistence, no encryption at rest, no
 * SecKeychainRef/SecKeychainItemRef opaque objects, no kSecReturnRef
 * support. Real Security.framework's keychain is an encrypted
 * SQLite-backed store (securityd's keychain-2.db) shared across processes
 * and surviving reboots; building that needs a working AES implementation
 * and an on-disk database format this tree doesn't have yet, so this is a
 * documented known limitation (same spirit as libdispatch's "no
 * dispatch_source_t" writeup), not a silent shortcut -- every item added
 * here is gone the moment the process exits.
 *
 * See SecItem.h for why the kSec* constants below are plain `CFStringRef`
 * globals populated by a constructor, not `const`.
 */
#include <Security/SecItem.h>
#include <pthread.h>
#include <stdlib.h>

CFStringRef kSecClass = (CFStringRef)0;
CFStringRef kSecClassGenericPassword = (CFStringRef)0;
CFStringRef kSecClassInternetPassword = (CFStringRef)0;
CFStringRef kSecClassCertificate = (CFStringRef)0;
CFStringRef kSecClassKey = (CFStringRef)0;
CFStringRef kSecClassIdentity = (CFStringRef)0;

CFStringRef kSecAttrService = (CFStringRef)0;
CFStringRef kSecAttrAccount = (CFStringRef)0;
CFStringRef kSecAttrLabel = (CFStringRef)0;

CFStringRef kSecValueData = (CFStringRef)0;

CFStringRef kSecReturnData = (CFStringRef)0;
CFStringRef kSecReturnAttributes = (CFStringRef)0;
CFStringRef kSecReturnRef = (CFStringRef)0;

CFStringRef kSecMatchLimit = (CFStringRef)0;
CFStringRef kSecMatchLimitOne = (CFStringRef)0;
CFStringRef kSecMatchLimitAll = (CFStringRef)0;

static CFStringRef mkstr(const char *s)
{
	return CFStringCreateWithCString(kCFAllocatorDefault, s, kCFStringEncodingUTF8);
}

/* Real Security.framework's four-char-code style attribute key values
 * (svce/acct/labl/...) -- kept for flavor/familiarity, not load-bearing:
 * nothing outside this dylib ever compares these strings by literal
 * value, only by CFEqual against the exported kSec* symbol itself. */
__attribute__((constructor))
static void initSecItemConstants(void)
{
	kSecClass = mkstr("class");
	kSecClassGenericPassword = mkstr("genp");
	kSecClassInternetPassword = mkstr("inet");
	kSecClassCertificate = mkstr("cert");
	kSecClassKey = mkstr("keys");
	kSecClassIdentity = mkstr("idnt");

	kSecAttrService = mkstr("svce");
	kSecAttrAccount = mkstr("acct");
	kSecAttrLabel = mkstr("labl");

	kSecValueData = mkstr("v_Data");

	kSecReturnData = mkstr("r_Data");
	kSecReturnAttributes = mkstr("r_Attributes");
	kSecReturnRef = mkstr("r_Ref");

	kSecMatchLimit = mkstr("m_Limit");
	kSecMatchLimitOne = mkstr("m_LimitOne");
	kSecMatchLimitAll = mkstr("m_LimitAll");
}

/* ---- in-memory item store ---- */

typedef struct SecKeychainItem {
	CFStringRef service; /* NULL == unset, not a wildcard on the stored item */
	CFStringRef account;
	CFStringRef label;
	CFDataRef data;
	struct SecKeychainItem *next;
} SecKeychainItem;

static SecKeychainItem *g_items;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static Boolean attrsEqual(CFStringRef a, CFStringRef b)
{
	if (a == NULL && b == NULL) {
		return true;
	}
	if (a == NULL || b == NULL) {
		return false;
	}
	return CFEqual(a, b);
}

static void freeItem(SecKeychainItem *item)
{
	if (item->service) {
		CFRelease(item->service);
	}
	if (item->account) {
		CFRelease(item->account);
	}
	if (item->label) {
		CFRelease(item->label);
	}
	if (item->data) {
		CFRelease(item->data);
	}
	free(item);
}

/* Walks the store starting after `after` (NULL to start from the head),
 * returning the first item matching every predicate the caller actually
 * supplied -- a NULL predicate is "don't filter on this attribute", same
 * as real SecItemCopyMatching's "omit the key to not narrow by it". */
static SecKeychainItem *findMatchLocked(CFStringRef service, CFStringRef account, CFStringRef label, SecKeychainItem *after)
{
	SecKeychainItem *it = after ? after->next : g_items;
	for (; it; it = it->next) {
		if (service && !attrsEqual(it->service, service)) {
			continue;
		}
		if (account && !attrsEqual(it->account, account)) {
			continue;
		}
		if (label && !attrsEqual(it->label, label)) {
			continue;
		}
		return it;
	}
	return NULL;
}

static OSStatus classFromDict(CFDictionaryRef dict, Boolean *outSupported)
{
	CFStringRef cls = (CFStringRef)CFDictionaryGetValue(dict, kSecClass);
	if (cls == NULL) {
		return errSecParam;
	}
	*outSupported = CFEqual(cls, kSecClassGenericPassword);
	return errSecSuccess;
}

static CFMutableDictionaryRef buildAttributesDict(const SecKeychainItem *item)
{
	CFMutableDictionaryRef d = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(d, kSecClass, kSecClassGenericPassword);
	if (item->service) {
		CFDictionarySetValue(d, kSecAttrService, item->service);
	}
	if (item->account) {
		CFDictionarySetValue(d, kSecAttrAccount, item->account);
	}
	if (item->label) {
		CFDictionarySetValue(d, kSecAttrLabel, item->label);
	}
	return d;
}

OSStatus SecItemAdd(CFDictionaryRef attributes, CFTypeRef *result)
{
	if (result) {
		*result = NULL;
	}
	if (attributes == NULL) {
		return errSecParam;
	}

	Boolean supported = false;
	OSStatus st = classFromDict(attributes, &supported);
	if (st != errSecSuccess) {
		return st;
	}
	if (!supported) {
		return errSecParam; /* v1: only kSecClassGenericPassword is implemented */
	}

	CFDataRef value = (CFDataRef)CFDictionaryGetValue(attributes, kSecValueData);
	if (value == NULL) {
		return errSecParam;
	}

	CFStringRef service = (CFStringRef)CFDictionaryGetValue(attributes, kSecAttrService);
	CFStringRef account = (CFStringRef)CFDictionaryGetValue(attributes, kSecAttrAccount);
	CFStringRef label = (CFStringRef)CFDictionaryGetValue(attributes, kSecAttrLabel);

	pthread_mutex_lock(&g_lock);

	/* Uniqueness is keyed on service+account, same as real generic-password
	 * items (ignoring accessgroup, which this v1 doesn't model). */
	if (findMatchLocked(service, account, NULL, NULL) != NULL) {
		pthread_mutex_unlock(&g_lock);
		return errSecDuplicateItem;
	}

	SecKeychainItem *item = calloc(1, sizeof(*item));
	if (item == NULL) {
		pthread_mutex_unlock(&g_lock);
		return errSecAllocate;
	}
	item->service = service ? CFStringCreateCopy(kCFAllocatorDefault, service) : NULL;
	item->account = account ? CFStringCreateCopy(kCFAllocatorDefault, account) : NULL;
	item->label = label ? CFStringCreateCopy(kCFAllocatorDefault, label) : NULL;
	item->data = CFDataCreateCopy(kCFAllocatorDefault, value);
	item->next = g_items;
	g_items = item;

	if (result && CFDictionaryGetValue(attributes, kSecReturnData) == (CFTypeRef)kCFBooleanTrue) {
		*result = CFDataCreateCopy(kCFAllocatorDefault, item->data);
	}

	pthread_mutex_unlock(&g_lock);
	return errSecSuccess;
}

OSStatus SecItemCopyMatching(CFDictionaryRef query, CFTypeRef *result)
{
	if (result) {
		*result = NULL;
	}
	if (query == NULL) {
		return errSecParam;
	}

	Boolean supported = false;
	OSStatus st = classFromDict(query, &supported);
	if (st != errSecSuccess) {
		return st;
	}
	if (!supported) {
		return errSecParam;
	}

	CFStringRef service = (CFStringRef)CFDictionaryGetValue(query, kSecAttrService);
	CFStringRef account = (CFStringRef)CFDictionaryGetValue(query, kSecAttrAccount);
	CFStringRef label = (CFStringRef)CFDictionaryGetValue(query, kSecAttrLabel);

	Boolean wantData = CFDictionaryGetValue(query, kSecReturnData) == (CFTypeRef)kCFBooleanTrue;
	Boolean wantAttrs = CFDictionaryGetValue(query, kSecReturnAttributes) == (CFTypeRef)kCFBooleanTrue;
	CFStringRef limit = (CFStringRef)CFDictionaryGetValue(query, kSecMatchLimit);
	Boolean matchAll = limit != NULL && CFEqual(limit, kSecMatchLimitAll);

	pthread_mutex_lock(&g_lock);

	if (!matchAll) {
		SecKeychainItem *item = findMatchLocked(service, account, label, NULL);
		if (item == NULL) {
			pthread_mutex_unlock(&g_lock);
			return errSecItemNotFound;
		}
		if (result && (wantData || wantAttrs)) {
			if (wantData && !wantAttrs) {
				*result = CFDataCreateCopy(kCFAllocatorDefault, item->data);
			} else if (wantAttrs && !wantData) {
				*result = buildAttributesDict(item);
			} else {
				CFMutableDictionaryRef d = buildAttributesDict(item);
				CFDictionarySetValue(d, kSecValueData, item->data);
				*result = d;
			}
		}
		pthread_mutex_unlock(&g_lock);
		return errSecSuccess;
	}

	CFMutableArrayRef results = result ? CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks) : NULL;
	SecKeychainItem *item = NULL;
	Boolean any = false;
	while ((item = findMatchLocked(service, account, label, item)) != NULL) {
		any = true;
		if (results) {
			CFTypeRef entry;
			if (wantData && !wantAttrs) {
				entry = CFDataCreateCopy(kCFAllocatorDefault, item->data);
			} else if (wantAttrs || wantData) {
				CFMutableDictionaryRef d = buildAttributesDict(item);
				if (wantData) {
					CFDictionarySetValue(d, kSecValueData, item->data);
				}
				entry = d;
			} else {
				entry = buildAttributesDict(item);
			}
			CFArrayAppendValue(results, entry);
			CFRelease(entry);
		}
	}

	pthread_mutex_unlock(&g_lock);

	if (!any) {
		if (results) {
			CFRelease(results);
		}
		return errSecItemNotFound;
	}
	if (result) {
		*result = results;
	}
	return errSecSuccess;
}

OSStatus SecItemUpdate(CFDictionaryRef query, CFDictionaryRef attributesToUpdate)
{
	if (query == NULL || attributesToUpdate == NULL) {
		return errSecParam;
	}

	Boolean supported = false;
	OSStatus st = classFromDict(query, &supported);
	if (st != errSecSuccess) {
		return st;
	}
	if (!supported) {
		return errSecParam;
	}

	CFStringRef service = (CFStringRef)CFDictionaryGetValue(query, kSecAttrService);
	CFStringRef account = (CFStringRef)CFDictionaryGetValue(query, kSecAttrAccount);
	CFStringRef label = (CFStringRef)CFDictionaryGetValue(query, kSecAttrLabel);

	CFDataRef newData = (CFDataRef)CFDictionaryGetValue(attributesToUpdate, kSecValueData);
	CFStringRef newLabel = (CFStringRef)CFDictionaryGetValue(attributesToUpdate, kSecAttrLabel);

	pthread_mutex_lock(&g_lock);

	SecKeychainItem *item = NULL;
	Boolean any = false;
	while ((item = findMatchLocked(service, account, label, item)) != NULL) {
		any = true;
		if (newData) {
			CFDataRef copy = CFDataCreateCopy(kCFAllocatorDefault, newData);
			if (item->data) {
				CFRelease(item->data);
			}
			item->data = copy;
		}
		if (newLabel) {
			CFStringRef copy = CFStringCreateCopy(kCFAllocatorDefault, newLabel);
			if (item->label) {
				CFRelease(item->label);
			}
			item->label = copy;
		}
	}

	pthread_mutex_unlock(&g_lock);
	return any ? errSecSuccess : errSecItemNotFound;
}

OSStatus SecItemDelete(CFDictionaryRef query)
{
	if (query == NULL) {
		return errSecParam;
	}

	Boolean supported = false;
	OSStatus st = classFromDict(query, &supported);
	if (st != errSecSuccess) {
		return st;
	}
	if (!supported) {
		return errSecParam;
	}

	CFStringRef service = (CFStringRef)CFDictionaryGetValue(query, kSecAttrService);
	CFStringRef account = (CFStringRef)CFDictionaryGetValue(query, kSecAttrAccount);
	CFStringRef label = (CFStringRef)CFDictionaryGetValue(query, kSecAttrLabel);

	pthread_mutex_lock(&g_lock);

	SecKeychainItem **link = &g_items;
	Boolean any = false;
	while (*link) {
		SecKeychainItem *it = *link;
		Boolean matches = (!service || attrsEqual(it->service, service))
			&& (!account || attrsEqual(it->account, account))
			&& (!label || attrsEqual(it->label, label));
		if (matches) {
			*link = it->next;
			freeItem(it);
			any = true;
		} else {
			link = &it->next;
		}
	}

	pthread_mutex_unlock(&g_lock);
	return any ? errSecSuccess : errSecItemNotFound;
}
