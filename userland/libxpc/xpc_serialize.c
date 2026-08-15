/* Copyright (c) 2026 Vihaan Nathan
 *
 * Our own recursive TLV wire format for xpc_object_t graphs -- not
 * Apple's actual bplist15 on-wire format, since nothing outside this OS's
 * own process pairs ever needs to read it (see xpc.h's scope note). One
 * byte type tag, then a type-specific payload; array/dictionary recurse.
 * All integers are raw host-endian (x86_64 LE) -- every peer on this OS
 * is the same architecture, there's no cross-endian wire to worry about.
 *
 * Encoding fails (returns false) if the object graph doesn't fit in `cap`
 * bytes (XPC_WIRE_MAX_PAYLOAD, from xpc_connection.c). Decoding treats
 * the buffer as untrusted -- it crossed a real process boundary over Mach
 * IPC -- and fails closed (returns NULL) on any truncated or malformed
 * input rather than reading past `len`.
 */
#include "xpc_internal.h"
#include <stdlib.h>
#include <string.h>

enum {
	WIRE_TAG_NULL = 1,
	WIRE_TAG_BOOL,
	WIRE_TAG_INT64,
	WIRE_TAG_UINT64,
	WIRE_TAG_DOUBLE,
	WIRE_TAG_STRING,
	WIRE_TAG_DATA,
	WIRE_TAG_ARRAY,
	WIRE_TAG_DICT,
};

struct writer {
	uint8_t *buf;
	size_t cap;
	size_t len;
	bool overflow;
};

static void
w_bytes(struct writer *w, const void *p, size_t n)
{
	if (w->overflow) {
		return;
	}
	if (n > w->cap - w->len) {
		w->overflow = true;
		return;
	}
	memcpy(w->buf + w->len, p, n);
	w->len += n;
}

static void w_u8(struct writer *w, uint8_t v) { w_bytes(w, &v, 1); }
static void w_u32(struct writer *w, uint32_t v) { w_bytes(w, &v, 4); }
static void w_u64(struct writer *w, uint64_t v) { w_bytes(w, &v, 8); }

static void
encode(struct writer *w, xpc_object_t obj)
{
	switch (obj->type->xt_tag) {
	case _XPC_TAG_NULL:
		w_u8(w, WIRE_TAG_NULL);
		break;
	case _XPC_TAG_BOOL:
		w_u8(w, WIRE_TAG_BOOL);
		w_u8(w, obj->u.b ? 1 : 0);
		break;
	case _XPC_TAG_INT64:
		w_u8(w, WIRE_TAG_INT64);
		w_u64(w, (uint64_t)obj->u.i64);
		break;
	case _XPC_TAG_UINT64:
		w_u8(w, WIRE_TAG_UINT64);
		w_u64(w, obj->u.u64);
		break;
	case _XPC_TAG_DOUBLE:
		w_u8(w, WIRE_TAG_DOUBLE);
		w_bytes(w, &obj->u.d, sizeof(double));
		break;
	case _XPC_TAG_STRING:
		w_u8(w, WIRE_TAG_STRING);
		w_u32(w, (uint32_t)obj->u.str.len);
		w_bytes(w, obj->u.str.bytes, obj->u.str.len);
		break;
	case _XPC_TAG_DATA:
		w_u8(w, WIRE_TAG_DATA);
		w_u32(w, (uint32_t)obj->u.data.len);
		w_bytes(w, obj->u.data.bytes, obj->u.data.len);
		break;
	case _XPC_TAG_ARRAY:
		w_u8(w, WIRE_TAG_ARRAY);
		w_u32(w, (uint32_t)obj->u.arr.count);
		for (size_t i = 0; i < obj->u.arr.count; i++) {
			encode(w, obj->u.arr.items[i]);
		}
		break;
	case _XPC_TAG_DICTIONARY:
		w_u8(w, WIRE_TAG_DICT);
		w_u32(w, (uint32_t)obj->u.dict.count);
		for (struct xpc_kv *kv = obj->u.dict.head; kv; kv = kv->next) {
			uint32_t klen = (uint32_t)strlen(kv->key);
			w_u32(w, klen);
			w_bytes(w, kv->key, klen);
			encode(w, kv->value);
		}
		break;
	default:
		/* connections/errors carry no serializable value -- a message
		 * containing one is a caller bug, fail the encode rather than
		 * silently dropping it. */
		w->overflow = true;
		break;
	}
}

bool
xpc_serialize(xpc_object_t obj, uint8_t *buf, size_t cap, size_t *out_len)
{
	struct writer w = { buf, cap, 0, false };
	encode(&w, obj);
	if (w.overflow) {
		return false;
	}
	*out_len = w.len;
	return true;
}

struct reader {
	const uint8_t *buf;
	size_t len;
	size_t pos;
	bool error;
};

static bool
r_bytes(struct reader *r, void *out, size_t n)
{
	if (r->error || n > r->len - r->pos) {
		r->error = true;
		return false;
	}
	memcpy(out, r->buf + r->pos, n);
	r->pos += n;
	return true;
}

static bool r_u8(struct reader *r, uint8_t *v) { return r_bytes(r, v, 1); }
static bool r_u32(struct reader *r, uint32_t *v) { return r_bytes(r, v, 4); }
static bool r_u64(struct reader *r, uint64_t *v) { return r_bytes(r, v, 8); }

static xpc_object_t
decode(struct reader *r)
{
	uint8_t tag;
	if (!r_u8(r, &tag)) {
		return NULL;
	}
	switch (tag) {
	case WIRE_TAG_NULL:
		return xpc_null_create();
	case WIRE_TAG_BOOL: {
		uint8_t v;
		if (!r_u8(r, &v)) {
			return NULL;
		}
		return xpc_bool_create(v != 0);
	}
	case WIRE_TAG_INT64: {
		uint64_t v;
		if (!r_u64(r, &v)) {
			return NULL;
		}
		return xpc_int64_create((int64_t)v);
	}
	case WIRE_TAG_UINT64: {
		uint64_t v;
		if (!r_u64(r, &v)) {
			return NULL;
		}
		return xpc_uint64_create(v);
	}
	case WIRE_TAG_DOUBLE: {
		double v;
		if (!r_bytes(r, &v, sizeof(double))) {
			return NULL;
		}
		return xpc_double_create(v);
	}
	case WIRE_TAG_STRING: {
		uint32_t len;
		if (!r_u32(r, &len) || len > r->len - r->pos) {
			r->error = true;
			return NULL;
		}
		xpc_object_t s = _xpc_string_create_len((const char *)(r->buf + r->pos), len);
		r->pos += len;
		return s;
	}
	case WIRE_TAG_DATA: {
		uint32_t len;
		if (!r_u32(r, &len) || len > r->len - r->pos) {
			r->error = true;
			return NULL;
		}
		xpc_object_t d = xpc_data_create(r->buf + r->pos, len);
		r->pos += len;
		return d;
	}
	case WIRE_TAG_ARRAY: {
		uint32_t count;
		if (!r_u32(r, &count)) {
			return NULL;
		}
		xpc_object_t a = xpc_array_create_empty();
		for (uint32_t i = 0; i < count; i++) {
			xpc_object_t v = decode(r);
			if (!v) {
				xpc_release(a);
				return NULL;
			}
			xpc_array_append_value(a, v);
			xpc_release(v);
		}
		return a;
	}
	case WIRE_TAG_DICT: {
		uint32_t count;
		if (!r_u32(r, &count)) {
			return NULL;
		}
		xpc_object_t d = xpc_dictionary_create_empty();
		for (uint32_t i = 0; i < count; i++) {
			uint32_t klen;
			if (!r_u32(r, &klen) || klen > r->len - r->pos) {
				xpc_release(d);
				return NULL;
			}
			char *key = malloc(klen + 1);
			memcpy(key, r->buf + r->pos, klen);
			key[klen] = '\0';
			r->pos += klen;
			xpc_object_t v = decode(r);
			if (!v) {
				free(key);
				xpc_release(d);
				return NULL;
			}
			xpc_dictionary_set_value(d, key, v);
			free(key);
			xpc_release(v);
		}
		return d;
	}
	default:
		r->error = true;
		return NULL;
	}
}

xpc_object_t
xpc_deserialize(const uint8_t *buf, size_t len)
{
	struct reader r = { buf, len, 0, false };
	return decode(&r);
}
