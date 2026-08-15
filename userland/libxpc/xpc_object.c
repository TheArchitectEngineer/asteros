/* Copyright (c) 2026 Vihaan Nathan
 *
 * Base refcounting + the scalar types (null/bool/int64/uint64/double/
 * string/data). Array and dictionary live in their own files since each
 * needs real recursive destroy/copy/equal logic; this file's
 * destroy/copy/equal dispatch calls into them for those two tags.
 */
#include "xpc_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const struct _xpc_type_s _xpc_type_null = { "null", _XPC_TAG_NULL };
const struct _xpc_type_s _xpc_type_bool = { "bool", _XPC_TAG_BOOL };
const struct _xpc_type_s _xpc_type_int64 = { "int64", _XPC_TAG_INT64 };
const struct _xpc_type_s _xpc_type_uint64 = { "uint64", _XPC_TAG_UINT64 };
const struct _xpc_type_s _xpc_type_double = { "double", _XPC_TAG_DOUBLE };
const struct _xpc_type_s _xpc_type_string = { "string", _XPC_TAG_STRING };
const struct _xpc_type_s _xpc_type_data = { "data", _XPC_TAG_DATA };
const struct _xpc_type_s _xpc_type_array = { "array", _XPC_TAG_ARRAY };
const struct _xpc_type_s _xpc_type_dictionary = { "dictionary", _XPC_TAG_DICTIONARY };
const struct _xpc_type_s _xpc_type_connection = { "connection", _XPC_TAG_CONNECTION };
const struct _xpc_type_s _xpc_type_error = { "error", _XPC_TAG_ERROR };

static const struct xpc_object_s s_err_interrupted = { 1, &_xpc_type_error, { .err_desc = "Connection interrupted" } };
static const struct xpc_object_s s_err_invalid = { 1, &_xpc_type_error, { .err_desc = "Connection invalid" } };
static const struct xpc_object_s s_err_imminent = { 1, &_xpc_type_error, { .err_desc = "Termination imminent" } };

xpc_object_t const XPC_ERROR_CONNECTION_INTERRUPTED = (xpc_object_t)&s_err_interrupted;
xpc_object_t const XPC_ERROR_CONNECTION_INVALID = (xpc_object_t)&s_err_invalid;
xpc_object_t const XPC_ERROR_TERMINATION_IMMINENT = (xpc_object_t)&s_err_imminent;

/* array.c / dictionary.c -- only this file's destroy/copy/equal dispatch
 * needs these, so they're not in xpc_internal.h's public-to-the-library
 * surface. */
void _xpc_array_destroy(xpc_object_t obj);
xpc_object_t _xpc_array_copy(xpc_object_t obj);
bool _xpc_array_equal(xpc_object_t a, xpc_object_t b);
void _xpc_dictionary_destroy(xpc_object_t obj);
xpc_object_t _xpc_dictionary_copy(xpc_object_t obj);
bool _xpc_dictionary_equal(xpc_object_t a, xpc_object_t b);
void _xpc_connection_destroy(struct xpc_connection_data *cd);

xpc_object_t
_xpc_object_alloc(xpc_type_t type)
{
	xpc_object_t obj = calloc(1, sizeof(*obj));
	obj->refcnt = 1;
	obj->type = type;
	return obj;
}

xpc_connection_t
_xpc_connection_alloc(void)
{
	xpc_connection_t conn = _xpc_object_alloc(XPC_TYPE_CONNECTION);
	conn->u.conn = calloc(1, sizeof(*conn->u.conn));
	pthread_mutex_init(&conn->u.conn->lock, NULL);
	pthread_cond_init(&conn->u.conn->resume_cond, NULL);
	return conn;
}

xpc_object_t
_xpc_string_create_len(const char *bytes, size_t len)
{
	xpc_object_t obj = _xpc_object_alloc(XPC_TYPE_STRING);
	obj->u.str.bytes = malloc(len + 1);
	memcpy(obj->u.str.bytes, bytes, len);
	obj->u.str.bytes[len] = '\0';
	obj->u.str.len = len;
	return obj;
}

static bool
is_static_singleton(xpc_object_t object)
{
	return object == XPC_ERROR_CONNECTION_INTERRUPTED ||
	    object == XPC_ERROR_CONNECTION_INVALID ||
	    object == XPC_ERROR_TERMINATION_IMMINENT;
}

xpc_object_t
xpc_retain(xpc_object_t object)
{
	if (!is_static_singleton(object)) {
		__atomic_add_fetch(&object->refcnt, 1, __ATOMIC_RELAXED);
	}
	return object;
}

void
xpc_release(xpc_object_t object)
{
	if (is_static_singleton(object)) {
		return;
	}
	if (__atomic_sub_fetch(&object->refcnt, 1, __ATOMIC_ACQ_REL) != 0) {
		return;
	}

	switch (object->type->xt_tag) {
	case _XPC_TAG_STRING:
		free(object->u.str.bytes);
		break;
	case _XPC_TAG_DATA:
		free(object->u.data.bytes);
		break;
	case _XPC_TAG_ARRAY:
		_xpc_array_destroy(object);
		break;
	case _XPC_TAG_DICTIONARY:
		_xpc_dictionary_destroy(object);
		break;
	case _XPC_TAG_CONNECTION:
		_xpc_connection_destroy(object->u.conn);
		free(object->u.conn);
		break;
	default:
		break;
	}
	free(object);
}

xpc_type_t
xpc_get_type(xpc_object_t object)
{
	return object->type;
}

const char *
xpc_type_get_name(xpc_type_t type)
{
	return type->xt_name;
}

xpc_object_t
xpc_copy(xpc_object_t object)
{
	switch (object->type->xt_tag) {
	case _XPC_TAG_NULL:
		return xpc_null_create();
	case _XPC_TAG_BOOL:
		return xpc_bool_create(object->u.b);
	case _XPC_TAG_INT64:
		return xpc_int64_create(object->u.i64);
	case _XPC_TAG_UINT64:
		return xpc_uint64_create(object->u.u64);
	case _XPC_TAG_DOUBLE:
		return xpc_double_create(object->u.d);
	case _XPC_TAG_STRING:
		return _xpc_string_create_len(object->u.str.bytes, object->u.str.len);
	case _XPC_TAG_DATA:
		return xpc_data_create(object->u.data.bytes, object->u.data.len);
	case _XPC_TAG_ARRAY:
		return _xpc_array_copy(object);
	case _XPC_TAG_DICTIONARY:
		return _xpc_dictionary_copy(object);
	default:
		/* connections/errors aren't copyable -- return the same
		 * reference, matching real xpc_copy's documented behavior for
		 * non-value types. */
		return xpc_retain(object);
	}
}

bool
xpc_equal(xpc_object_t object1, xpc_object_t object2)
{
	if (object1 == object2) {
		return true;
	}
	if (object1->type != object2->type) {
		return false;
	}
	switch (object1->type->xt_tag) {
	case _XPC_TAG_NULL:
		return true;
	case _XPC_TAG_BOOL:
		return object1->u.b == object2->u.b;
	case _XPC_TAG_INT64:
		return object1->u.i64 == object2->u.i64;
	case _XPC_TAG_UINT64:
		return object1->u.u64 == object2->u.u64;
	case _XPC_TAG_DOUBLE:
		return object1->u.d == object2->u.d;
	case _XPC_TAG_STRING:
		return object1->u.str.len == object2->u.str.len &&
		    memcmp(object1->u.str.bytes, object2->u.str.bytes, object1->u.str.len) == 0;
	case _XPC_TAG_DATA:
		return object1->u.data.len == object2->u.data.len &&
		    memcmp(object1->u.data.bytes, object2->u.data.bytes, object1->u.data.len) == 0;
	case _XPC_TAG_ARRAY:
		return _xpc_array_equal(object1, object2);
	case _XPC_TAG_DICTIONARY:
		return _xpc_dictionary_equal(object1, object2);
	default:
		return false;
	}
}

struct desc_buf {
	char *buf;
	size_t len;
	size_t cap;
};

static void
desc_append(struct desc_buf *d, const char *s)
{
	size_t n = strlen(s);
	if (d->len + n + 1 > d->cap) {
		d->cap = (d->len + n + 1) * 2;
		d->buf = realloc(d->buf, d->cap);
	}
	memcpy(d->buf + d->len, s, n + 1);
	d->len += n;
}

static void
desc_object(struct desc_buf *d, xpc_object_t object)
{
	char tmp[128];
	switch (object->type->xt_tag) {
	case _XPC_TAG_NULL:
		desc_append(d, "<null>");
		break;
	case _XPC_TAG_BOOL:
		desc_append(d, object->u.b ? "true" : "false");
		break;
	case _XPC_TAG_INT64:
		snprintf(tmp, sizeof(tmp), "%lld", (long long)object->u.i64);
		desc_append(d, tmp);
		break;
	case _XPC_TAG_UINT64:
		snprintf(tmp, sizeof(tmp), "%llu", (unsigned long long)object->u.u64);
		desc_append(d, tmp);
		break;
	case _XPC_TAG_DOUBLE:
		snprintf(tmp, sizeof(tmp), "%g", object->u.d);
		desc_append(d, tmp);
		break;
	case _XPC_TAG_STRING:
		desc_append(d, "\"");
		desc_append(d, object->u.str.bytes);
		desc_append(d, "\"");
		break;
	case _XPC_TAG_DATA:
		snprintf(tmp, sizeof(tmp), "<data: %zu bytes>", object->u.data.len);
		desc_append(d, tmp);
		break;
	case _XPC_TAG_ARRAY:
		desc_append(d, "[");
		for (size_t i = 0; i < object->u.arr.count; i++) {
			if (i) {
				desc_append(d, ", ");
			}
			desc_object(d, object->u.arr.items[i]);
		}
		desc_append(d, "]");
		break;
	case _XPC_TAG_DICTIONARY:
		desc_append(d, "{");
		bool first = true;
		for (struct xpc_kv *kv = object->u.dict.head; kv; kv = kv->next) {
			if (!first) {
				desc_append(d, ", ");
			}
			first = false;
			desc_append(d, kv->key);
			desc_append(d, " = ");
			desc_object(d, kv->value);
		}
		desc_append(d, "}");
		break;
	case _XPC_TAG_CONNECTION:
		desc_append(d, "<connection>");
		break;
	case _XPC_TAG_ERROR:
		desc_append(d, "<error: ");
		desc_append(d, object->u.err_desc);
		desc_append(d, ">");
		break;
	default:
		desc_append(d, "<?>");
		break;
	}
}

char *
xpc_copy_description(xpc_object_t object)
{
	struct desc_buf d = { malloc(64), 0, 64 };
	d.buf[0] = '\0';
	desc_object(&d, object);
	return d.buf;
}

/* ---- scalars ---- */

xpc_object_t
xpc_null_create(void)
{
	return _xpc_object_alloc(XPC_TYPE_NULL);
}

xpc_object_t
xpc_bool_create(bool value)
{
	xpc_object_t obj = _xpc_object_alloc(XPC_TYPE_BOOL);
	obj->u.b = value;
	return obj;
}

bool
xpc_bool_get_value(xpc_object_t xbool)
{
	return xbool->u.b;
}

xpc_object_t
xpc_int64_create(int64_t value)
{
	xpc_object_t obj = _xpc_object_alloc(XPC_TYPE_INT64);
	obj->u.i64 = value;
	return obj;
}

int64_t
xpc_int64_get_value(xpc_object_t xint)
{
	return xint->u.i64;
}

xpc_object_t
xpc_uint64_create(uint64_t value)
{
	xpc_object_t obj = _xpc_object_alloc(XPC_TYPE_UINT64);
	obj->u.u64 = value;
	return obj;
}

uint64_t
xpc_uint64_get_value(xpc_object_t xuint)
{
	return xuint->u.u64;
}

xpc_object_t
xpc_double_create(double value)
{
	xpc_object_t obj = _xpc_object_alloc(XPC_TYPE_DOUBLE);
	obj->u.d = value;
	return obj;
}

double
xpc_double_get_value(xpc_object_t xdouble)
{
	return xdouble->u.d;
}

xpc_object_t
xpc_string_create(const char *string)
{
	return _xpc_string_create_len(string, strlen(string));
}

size_t
xpc_string_get_length(xpc_object_t xstring)
{
	return xstring->u.str.len;
}

const char *
xpc_string_get_string_ptr(xpc_object_t xstring)
{
	return xstring->u.str.bytes;
}

xpc_object_t
xpc_data_create(const void *bytes, size_t length)
{
	xpc_object_t obj = _xpc_object_alloc(XPC_TYPE_DATA);
	obj->u.data.bytes = malloc(length ? length : 1);
	if (length) {
		memcpy(obj->u.data.bytes, bytes, length);
	}
	obj->u.data.len = length;
	return obj;
}

size_t
xpc_data_get_length(xpc_object_t xdata)
{
	return xdata->u.data.len;
}

const void *
xpc_data_get_bytes_ptr(xpc_object_t xdata)
{
	return xdata->u.data.bytes;
}

size_t
xpc_data_get_bytes(xpc_object_t xdata, void *buffer, size_t off, size_t length)
{
	if (off >= xdata->u.data.len) {
		return 0;
	}
	size_t avail = xdata->u.data.len - off;
	size_t n = length < avail ? length : avail;
	memcpy(buffer, xdata->u.data.bytes + off, n);
	return n;
}
