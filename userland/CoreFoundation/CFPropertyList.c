/* Copyright (c) 2026 Vihaan Nathan
 *
 * XML property list (de)serialization -- see CFPropertyList.h for exact
 * scope. Written from scratch against Apple's real plist DTD shape
 * (<plist version="1.0"><dict>/<array>/<string>/<integer>/<real>/
 * <true/>/<false/>/<data>...), not vendored, since this is genuinely new
 * functionality this OS needs (Phase 25's config.defs wire format) rather
 * than something with a natural single upstream .c file to port -- real
 * CF's own XML plist code is deeply entangled with CFRunLoop-adjacent
 * internal machinery this project doesn't have.
 */
#include "CFInternal.h"
#include <CoreFoundation/CFPropertyList.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFNumber.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ==== base64 (RFC 4648, standard alphabet) ==== */

static const char b64_alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void b64Encode(CFMutableDataRef out, const UInt8 *bytes, CFIndex length)
{
	CFIndex i = 0;
	int col = 0;
	while (i + 3 <= length) {
		UInt32 v = ((UInt32)bytes[i] << 16) | ((UInt32)bytes[i + 1] << 8) | bytes[i + 2];
		UInt8 quad[4] = {
			(UInt8)b64_alphabet[(v >> 18) & 0x3f], (UInt8)b64_alphabet[(v >> 12) & 0x3f],
			(UInt8)b64_alphabet[(v >> 6) & 0x3f], (UInt8)b64_alphabet[v & 0x3f]
		};
		CFDataAppendBytes(out, quad, 4);
		i += 3;
		col += 4;
		if (col >= 76) {
			CFDataAppendBytes(out, (const UInt8 *)"\n", 1);
			col = 0;
		}
	}
	CFIndex rem = length - i;
	if (rem == 1) {
		UInt32 v = (UInt32)bytes[i] << 16;
		UInt8 quad[4] = { (UInt8)b64_alphabet[(v >> 18) & 0x3f], (UInt8)b64_alphabet[(v >> 12) & 0x3f], '=', '=' };
		CFDataAppendBytes(out, quad, 4);
	} else if (rem == 2) {
		UInt32 v = ((UInt32)bytes[i] << 16) | ((UInt32)bytes[i + 1] << 8);
		UInt8 quad[4] = { (UInt8)b64_alphabet[(v >> 18) & 0x3f], (UInt8)b64_alphabet[(v >> 12) & 0x3f],
			(UInt8)b64_alphabet[(v >> 6) & 0x3f], '=' };
		CFDataAppendBytes(out, quad, 4);
	}
}

static int b64Value(char c)
{
	if (c >= 'A' && c <= 'Z') return c - 'A';
	if (c >= 'a' && c <= 'z') return c - 'a' + 26;
	if (c >= '0' && c <= '9') return c - '0' + 52;
	if (c == '+') return 62;
	if (c == '/') return 63;
	return -1;
}

/* Decodes base64 text (whitespace/newlines interspersed are skipped, same
 * as any real plist writer's line-wrapped <data> content) into bytes. */
static CFDataRef b64Decode(CFAllocatorRef allocator, const char *text, CFIndex len)
{
	CFMutableDataRef out = CFDataCreateMutable(allocator, (CFIndex)(len * 3 / 4 + 4));
	int vals[4];
	int n = 0;
	for (CFIndex i = 0; i < len; i++) {
		char c = text[i];
		if (c == '=' || c == '\n' || c == '\r' || c == ' ' || c == '\t')
			continue;
		int v = b64Value(c);
		if (v < 0)
			continue;
		vals[n++] = v;
		if (n == 4) {
			UInt8 bytes[3] = {
				(UInt8)((vals[0] << 2) | (vals[1] >> 4)),
				(UInt8)((vals[1] << 4) | (vals[2] >> 2)),
				(UInt8)((vals[2] << 6) | vals[3])
			};
			CFDataAppendBytes(out, bytes, 3);
			n = 0;
		}
	}
	if (n >= 2) {
		UInt8 b0 = (UInt8)((vals[0] << 2) | (vals[1] >> 4));
		CFDataAppendBytes(out, &b0, 1);
		if (n == 3) {
			UInt8 b1 = (UInt8)((vals[1] << 4) | (vals[2] >> 2));
			CFDataAppendBytes(out, &b1, 1);
		}
	}
	return (CFDataRef)out;
}

/* ==== writer ==== */

static void appendCStr(CFMutableDataRef out, const char *s)
{
	CFDataAppendBytes(out, (const UInt8 *)s, (CFIndex)strlen(s));
}

static void appendIndent(CFMutableDataRef out, int depth)
{
	for (int i = 0; i < depth; i++)
		appendCStr(out, "\t");
}

/* Escapes the five XML predefined entities. Property list text content
 * (keys, <string> values) is never anything but well-formed UTF-8 already
 * (CFString's own internal storage), so nothing beyond entity-escaping is
 * needed here. */
static void appendEscaped(CFMutableDataRef out, const char *utf8)
{
	for (const char *p = utf8; *p; p++) {
		switch (*p) {
		case '&': appendCStr(out, "&amp;"); break;
		case '<': appendCStr(out, "&lt;"); break;
		case '>': appendCStr(out, "&gt;"); break;
		case '"': appendCStr(out, "&quot;"); break;
		case '\'': appendCStr(out, "&apos;"); break;
		default: CFDataAppendBytes(out, (const UInt8 *)p, 1); break;
		}
	}
}

static void appendString(CFMutableDataRef out, const char *tag, CFStringRef s)
{
	appendCStr(out, "<");
	appendCStr(out, tag);
	appendCStr(out, ">");
	const char *cstr = CFStringGetCStringPtr(s, kCFStringEncodingUTF8);
	if (cstr)
		appendEscaped(out, cstr);
	appendCStr(out, "</");
	appendCStr(out, tag);
	appendCStr(out, ">\n");
}

struct dictWriteCtx {
	CFMutableDataRef out;
	int depth;
	Boolean ok;
};

static Boolean writeValue(CFMutableDataRef out, CFPropertyListRef value, int depth);

static void dictWriteApplier(const void *key, const void *val, void *ctx)
{
	struct dictWriteCtx *c = (struct dictWriteCtx *)ctx;
	if (!c->ok)
		return;
	if (CFGetTypeID((CFTypeRef)key) != CFStringGetTypeID()) {
		c->ok = false;
		return;
	}
	appendIndent(c->out, c->depth);
	appendString(c->out, "key", (CFStringRef)key);
	if (!writeValue(c->out, val, c->depth)) {
		c->ok = false;
	}
}

static Boolean writeValue(CFMutableDataRef out, CFPropertyListRef value, int depth)
{
	CFTypeID tid = CFGetTypeID(value);

	if (tid == CFBooleanGetTypeID()) {
		appendIndent(out, depth);
		appendCStr(out, CFBooleanGetValue((CFBooleanRef)value) ? "<true/>\n" : "<false/>\n");
		return true;
	}
	if (tid == CFStringGetTypeID()) {
		appendIndent(out, depth);
		appendString(out, "string", (CFStringRef)value);
		return true;
	}
	if (tid == CFNumberGetTypeID()) {
		CFNumberRef n = (CFNumberRef)value;
		appendIndent(out, depth);
		if (CFNumberIsFloatType(n)) {
			double d;
			CFNumberGetValue(n, kCFNumberDoubleType, &d);
			char buf[64];
			snprintf(buf, sizeof(buf), "<real>%.17g</real>\n", d);
			appendCStr(out, buf);
		} else {
			long long v;
			CFNumberGetValue(n, kCFNumberSInt64Type, &v);
			char buf[32];
			snprintf(buf, sizeof(buf), "<integer>%lld</integer>\n", v);
			appendCStr(out, buf);
		}
		return true;
	}
	if (tid == CFDataGetTypeID()) {
		CFDataRef d = (CFDataRef)value;
		appendIndent(out, depth);
		appendCStr(out, "<data>\n");
		b64Encode(out, CFDataGetBytePtr(d), CFDataGetLength(d));
		appendCStr(out, "\n");
		appendIndent(out, depth);
		appendCStr(out, "</data>\n");
		return true;
	}
	if (tid == CFArrayGetTypeID()) {
		CFArrayRef a = (CFArrayRef)value;
		appendIndent(out, depth);
		appendCStr(out, "<array>\n");
		CFIndex n = CFArrayGetCount(a);
		for (CFIndex i = 0; i < n; i++) {
			if (!writeValue(out, CFArrayGetValueAtIndex(a, i), depth + 1))
				return false;
		}
		appendIndent(out, depth);
		appendCStr(out, "</array>\n");
		return true;
	}
	if (tid == CFDictionaryGetTypeID()) {
		appendIndent(out, depth);
		appendCStr(out, "<dict>\n");
		struct dictWriteCtx ctx = { out, depth + 1, true };
		CFDictionaryApplyFunction((CFDictionaryRef)value, dictWriteApplier, &ctx);
		if (!ctx.ok)
			return false;
		appendIndent(out, depth);
		appendCStr(out, "</dict>\n");
		return true;
	}
	return false;
}

CFDataRef CFPropertyListCreateXMLData(CFAllocatorRef allocator, CFPropertyListRef propertyList)
{
	CFMutableDataRef out = CFDataCreateMutable(allocator, 256);
	appendCStr(out, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	appendCStr(out, "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n");
	appendCStr(out, "<plist version=\"1.0\">\n");
	if (!writeValue(out, propertyList, 0)) {
		CFRelease(out);
		return NULL;
	}
	appendCStr(out, "</plist>\n");
	return (CFDataRef)out;
}

/* ==== reader ==== */

struct cursor {
	const char *p;
	const char *end;
	CFAllocatorRef allocator;
	CFPropertyListMutabilityOptions mutability;
};

static void setError(CFStringRef *errorString, const char *msg)
{
	if (errorString)
		*errorString = CFStringCreateWithCString(kCFAllocatorDefault, msg, kCFStringEncodingUTF8);
}

static void skipWs(struct cursor *c)
{
	while (c->p < c->end && (*c->p == ' ' || *c->p == '\t' || *c->p == '\n' || *c->p == '\r'))
		c->p++;
}

/* Skips a `<?...?>` or `<!...>` directive (XML prolog / DOCTYPE) -- both
 * are single well-formed runs with no nested '>' in a real plist's
 * prolog, so scanning to the next unescaped '>' is sufficient. */
static void skipDirective(struct cursor *c)
{
	while (c->p < c->end && *c->p != '>')
		c->p++;
	if (c->p < c->end)
		c->p++;
}

static void skipProlog(struct cursor *c)
{
	for (;;) {
		skipWs(c);
		if (c->p + 1 < c->end && c->p[0] == '<' && (c->p[1] == '?' || c->p[1] == '!')) {
			skipDirective(c);
			continue;
		}
		break;
	}
}

/* Reads one XML text/entity run up to (not including) the next '<',
 * unescaping the five predefined entities. Returns a malloc'd, NUL
 * terminated buffer the caller frees. */
static char *readText(struct cursor *c, CFIndex *outLen)
{
	const char *start = c->p;
	while (c->p < c->end && *c->p != '<')
		c->p++;
	CFIndex rawLen = (CFIndex)(c->p - start);
	char *buf = malloc((size_t)rawLen + 1);
	CFIndex o = 0;
	for (CFIndex i = 0; i < rawLen; i++) {
		char ch = start[i];
		if (ch == '&') {
			if (rawLen - i >= 5 && memcmp(start + i, "&amp;", 5) == 0) { buf[o++] = '&'; i += 4; continue; }
			if (rawLen - i >= 4 && memcmp(start + i, "&lt;", 4) == 0) { buf[o++] = '<'; i += 3; continue; }
			if (rawLen - i >= 4 && memcmp(start + i, "&gt;", 4) == 0) { buf[o++] = '>'; i += 3; continue; }
			if (rawLen - i >= 6 && memcmp(start + i, "&quot;", 6) == 0) { buf[o++] = '"'; i += 5; continue; }
			if (rawLen - i >= 6 && memcmp(start + i, "&apos;", 6) == 0) { buf[o++] = '\''; i += 5; continue; }
		}
		buf[o++] = ch;
	}
	buf[o] = 0;
	if (outLen)
		*outLen = o;
	return buf;
}

/* Reads `<tagname` (already past '<') up to the closing '>' or '/>',
 * returning the bare tag name (attributes, if any, are discarded --
 * nothing this reader needs to parse carries plist-meaningful attributes
 * beyond <plist version="..."> itself, which is validated by name only). */
static char *readTagName(struct cursor *c, Boolean *selfClosing)
{
	const char *start = c->p;
	while (c->p < c->end && *c->p != ' ' && *c->p != '\t' && *c->p != '\n' &&
	    *c->p != '>' && *c->p != '/')
		c->p++;
	CFIndex nameLen = (CFIndex)(c->p - start);
	char *name = malloc((size_t)nameLen + 1);
	memcpy(name, start, (size_t)nameLen);
	name[nameLen] = 0;

	*selfClosing = false;
	while (c->p < c->end && *c->p != '>') {
		if (*c->p == '/')
			*selfClosing = true;
		c->p++;
	}
	if (c->p < c->end)
		c->p++; /* consume '>' */
	return name;
}

static Boolean expectCloseTag(struct cursor *c, const char *name)
{
	skipWs(c);
	if (c->p >= c->end || *c->p != '<')
		return false;
	c->p++;
	if (c->p >= c->end || *c->p != '/')
		return false;
	c->p++;
	size_t len = strlen(name);
	if ((size_t)(c->end - c->p) < len || memcmp(c->p, name, len) != 0)
		return false;
	c->p += len;
	skipWs(c);
	if (c->p >= c->end || *c->p != '>')
		return false;
	c->p++;
	return true;
}

static CFPropertyListRef parseValue(struct cursor *c, CFStringRef *errorString);

static CFPropertyListRef parseDict(struct cursor *c, CFStringRef *errorString)
{
	CFMutableDictionaryRef d = CFDictionaryCreateMutable(c->allocator, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	for (;;) {
		skipWs(c);
		if (c->p + 1 < c->end && c->p[0] == '<' && c->p[1] == '/')
			break;
		if (c->p >= c->end || *c->p != '<') {
			setError(errorString, "expected <key> or </dict>");
			CFRelease(d);
			return NULL;
		}
		c->p++;
		Boolean selfClose;
		char *tag = readTagName(c, &selfClose);
		Boolean isKey = strcmp(tag, "key") == 0;
		free(tag);
		if (!isKey) {
			setError(errorString, "expected <key> inside <dict>");
			CFRelease(d);
			return NULL;
		}
		CFIndex keyLen;
		char *keyText = readText(c, &keyLen);
		if (!expectCloseTag(c, "key")) {
			free(keyText);
			setError(errorString, "unterminated <key>");
			CFRelease(d);
			return NULL;
		}
		CFStringRef key = CFStringCreateWithBytes(c->allocator, (const UInt8 *)keyText, keyLen, kCFStringEncodingUTF8, false);
		free(keyText);

		CFPropertyListRef val = parseValue(c, errorString);
		if (!val) {
			CFRelease(key);
			CFRelease(d);
			return NULL;
		}
		CFDictionarySetValue(d, key, val);
		CFRelease(key);
		CFRelease(val);
	}
	if (!expectCloseTag(c, "dict")) {
		setError(errorString, "unterminated <dict>");
		CFRelease(d);
		return NULL;
	}
	if (c->mutability == kCFPropertyListImmutable) {
		CFDictionaryRef immutable = CFDictionaryCreateCopy(c->allocator, d);
		CFRelease(d);
		return immutable;
	}
	return d;
}

static CFPropertyListRef parseArray(struct cursor *c, CFStringRef *errorString)
{
	CFMutableArrayRef a = CFArrayCreateMutable(c->allocator, 0, &kCFTypeArrayCallBacks);
	for (;;) {
		skipWs(c);
		if (c->p + 1 < c->end && c->p[0] == '<' && c->p[1] == '/')
			break;
		CFPropertyListRef val = parseValue(c, errorString);
		if (!val) {
			CFRelease(a);
			return NULL;
		}
		CFArrayAppendValue(a, val);
		CFRelease(val);
	}
	if (!expectCloseTag(c, "array")) {
		setError(errorString, "unterminated <array>");
		CFRelease(a);
		return NULL;
	}
	return a;
}

static CFPropertyListRef parseValue(struct cursor *c, CFStringRef *errorString)
{
	skipWs(c);
	if (c->p >= c->end || *c->p != '<') {
		setError(errorString, "expected a value");
		return NULL;
	}
	c->p++;
	Boolean selfClose;
	char *tag = readTagName(c, &selfClose);
	CFPropertyListRef result = NULL;

	if (strcmp(tag, "dict") == 0) {
		result = parseDict(c, errorString);
	} else if (strcmp(tag, "array") == 0) {
		result = parseArray(c, errorString);
	} else if (strcmp(tag, "true") == 0) {
		result = CFRetain(kCFBooleanTrue);
	} else if (strcmp(tag, "false") == 0) {
		result = CFRetain(kCFBooleanFalse);
	} else if (strcmp(tag, "string") == 0) {
		CFIndex len;
		char *text = readText(c, &len);
		if (expectCloseTag(c, "string")) {
			result = CFStringCreateWithBytes(c->allocator, (const UInt8 *)text, len, kCFStringEncodingUTF8, false);
		} else {
			setError(errorString, "unterminated <string>");
		}
		free(text);
	} else if (strcmp(tag, "integer") == 0) {
		CFIndex len;
		char *text = readText(c, &len);
		if (expectCloseTag(c, "integer")) {
			long long v = strtoll(text, NULL, 10);
			result = CFNumberCreate(c->allocator, kCFNumberSInt64Type, &v);
		} else {
			setError(errorString, "unterminated <integer>");
		}
		free(text);
	} else if (strcmp(tag, "real") == 0) {
		CFIndex len;
		char *text = readText(c, &len);
		if (expectCloseTag(c, "real")) {
			double v = strtod(text, NULL);
			result = CFNumberCreate(c->allocator, kCFNumberDoubleType, &v);
		} else {
			setError(errorString, "unterminated <real>");
		}
		free(text);
	} else if (strcmp(tag, "data") == 0) {
		CFIndex len;
		char *text = readText(c, &len);
		if (expectCloseTag(c, "data")) {
			result = b64Decode(c->allocator, text, len);
		} else {
			setError(errorString, "unterminated <data>");
		}
		free(text);
	} else {
		setError(errorString, "unrecognized plist element");
	}

	free(tag);
	return result;
}

CFPropertyListRef CFPropertyListCreateFromXMLData(CFAllocatorRef allocator, CFDataRef xmlData, CFPropertyListMutabilityOptions mutabilityOption, CFStringRef *errorString)
{
	if (errorString)
		*errorString = NULL;

	struct cursor c;
	c.p = (const char *)CFDataGetBytePtr(xmlData);
	c.end = c.p + CFDataGetLength(xmlData);
	c.allocator = allocator;
	c.mutability = mutabilityOption;

	skipProlog(&c);
	skipWs(&c);
	if (c.p >= c.end || *c.p != '<') {
		setError(errorString, "missing <plist>");
		return NULL;
	}
	c.p++;
	Boolean selfClose;
	char *tag = readTagName(&c, &selfClose);
	Boolean isPlist = strcmp(tag, "plist") == 0;
	free(tag);
	if (!isPlist) {
		setError(errorString, "expected <plist>");
		return NULL;
	}

	CFPropertyListRef result = parseValue(&c, errorString);
	if (!result)
		return NULL;

	if (!expectCloseTag(&c, "plist")) {
		setError(errorString, "unterminated <plist>");
		CFRelease(result);
		return NULL;
	}
	return result;
}

Boolean CFPropertyListIsValid(CFPropertyListRef plist, CFStringRef xmlPropertyListVersion)
{
	(void)xmlPropertyListVersion;
	CFTypeID tid = CFGetTypeID(plist);
	if (tid == CFBooleanGetTypeID() || tid == CFStringGetTypeID() ||
	    tid == CFNumberGetTypeID() || tid == CFDataGetTypeID())
		return true;
	if (tid == CFArrayGetTypeID()) {
		CFArrayRef a = (CFArrayRef)plist;
		CFIndex n = CFArrayGetCount(a);
		for (CFIndex i = 0; i < n; i++)
			if (!CFPropertyListIsValid(CFArrayGetValueAtIndex(a, i), xmlPropertyListVersion))
				return false;
		return true;
	}
	if (tid == CFDictionaryGetTypeID()) {
		CFDictionaryRef d = (CFDictionaryRef)plist;
		CFIndex n = CFDictionaryGetCount(d);
		const void **keys = malloc(sizeof(void *) * (size_t)(n ? n : 1));
		const void **vals = malloc(sizeof(void *) * (size_t)(n ? n : 1));
		CFDictionaryGetKeysAndValues(d, keys, vals);
		Boolean ok = true;
		for (CFIndex i = 0; i < n; i++) {
			if (CFGetTypeID(keys[i]) != CFStringGetTypeID() || !CFPropertyListIsValid(vals[i], xmlPropertyListVersion)) {
				ok = false;
				break;
			}
		}
		free(keys);
		free(vals);
		return ok;
	}
	return false;
}

/* kCFPropertyListXMLFormatVersion1_0 is filled in by CFString's own
 * runtime, not this file's -- CFStringCreateWithCString isn't a
 * constant expression, so (matching kCFBooleanTrue/False and kCFNull's
 * own pattern in CFBoolean.c/CFNull.c) this can't be a static file-scope
 * initializer. Unlike those two, though, nothing here needs it valid
 * before other CF calls run (it's just a version-string constant, not a
 * self-describing runtime instance), so plain constructor-order-agnostic
 * lazy init on first real use is enough -- no ordering dependency to
 * worry about. */
CFStringRef kCFPropertyListXMLFormatVersion1_0;

__attribute__((constructor))
static void propertyListInit(void)
{
	kCFPropertyListXMLFormatVersion1_0 = CFStringCreateWithCString(kCFAllocatorDefault, "1.0", kCFStringEncodingUTF8);
}
