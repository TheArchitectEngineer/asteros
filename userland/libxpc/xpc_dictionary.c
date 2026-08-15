/* Copyright (c) 2026 Vihaan Nathan
 *
 * xpc_object_t dictionary: a plain singly-linked key/value list, not a
 * hash table -- fine at the sizes an IPC message payload actually needs
 * (capped at XPC_WIRE_MAX_PAYLOAD anyway, see xpc_internal.h), same "keep
 * it simple, not fake" tradeoff userland/libdispatch's own runnable-queue
 * linked list already makes for this codebase.
 */
#include "xpc_internal.h"
#include <stdlib.h>
#include <string.h>

xpc_object_t
xpc_dictionary_create_empty(void)
{
	xpc_object_t obj = _xpc_object_alloc(XPC_TYPE_DICTIONARY);
	obj->u.dict.head = NULL;
	obj->u.dict.count = 0;
	obj->u.dict.assoc_conn = NULL;
	obj->u.dict.assoc_msgid = 0;
	return obj;
}

xpc_object_t
xpc_dictionary_create(const char * const *keys, const xpc_object_t *values, size_t count)
{
	xpc_object_t dict = xpc_dictionary_create_empty();
	for (size_t i = 0; i < count; i++) {
		xpc_dictionary_set_value(dict, keys[i], values[i]);
	}
	return dict;
}

xpc_object_t
xpc_dictionary_create_reply(xpc_object_t original)
{
	xpc_object_t reply = xpc_dictionary_create_empty();
	if (original->u.dict.assoc_conn) {
		reply->u.dict.assoc_conn = xpc_retain(original->u.dict.assoc_conn);
		reply->u.dict.assoc_msgid = original->u.dict.assoc_msgid;
	}
	return reply;
}

static struct xpc_kv *
find_kv(xpc_object_t xdict, const char *key)
{
	for (struct xpc_kv *kv = xdict->u.dict.head; kv; kv = kv->next) {
		if (strcmp(kv->key, key) == 0) {
			return kv;
		}
	}
	return NULL;
}

void
xpc_dictionary_set_value(xpc_object_t xdict, const char *key, xpc_object_t value)
{
	struct xpc_kv *kv = find_kv(xdict, key);
	if (kv) {
		xpc_release(kv->value);
		kv->value = xpc_retain(value);
		return;
	}
	kv = malloc(sizeof(*kv));
	kv->key = strdup(key);
	kv->value = xpc_retain(value);
	kv->next = xdict->u.dict.head;
	xdict->u.dict.head = kv;
	xdict->u.dict.count++;
}

xpc_object_t
xpc_dictionary_get_value(xpc_object_t xdict, const char *key)
{
	struct xpc_kv *kv = find_kv(xdict, key);
	return kv ? kv->value : NULL;
}

size_t
xpc_dictionary_get_count(xpc_object_t xdict)
{
	return xdict->u.dict.count;
}

bool
xpc_dictionary_apply(xpc_object_t xdict, xpc_dictionary_applier_t applier)
{
	for (struct xpc_kv *kv = xdict->u.dict.head; kv; kv = kv->next) {
		if (!applier(kv->key, kv->value)) {
			return false;
		}
	}
	return true;
}

xpc_connection_t
xpc_dictionary_get_remote_connection(xpc_object_t xdict)
{
	return xdict->u.dict.assoc_conn;
}

void
xpc_dictionary_set_bool(xpc_object_t xdict, const char *key, bool value)
{
	xpc_object_t v = xpc_bool_create(value);
	xpc_dictionary_set_value(xdict, key, v);
	xpc_release(v);
}

void
xpc_dictionary_set_int64(xpc_object_t xdict, const char *key, int64_t value)
{
	xpc_object_t v = xpc_int64_create(value);
	xpc_dictionary_set_value(xdict, key, v);
	xpc_release(v);
}

void
xpc_dictionary_set_uint64(xpc_object_t xdict, const char *key, uint64_t value)
{
	xpc_object_t v = xpc_uint64_create(value);
	xpc_dictionary_set_value(xdict, key, v);
	xpc_release(v);
}

void
xpc_dictionary_set_double(xpc_object_t xdict, const char *key, double value)
{
	xpc_object_t v = xpc_double_create(value);
	xpc_dictionary_set_value(xdict, key, v);
	xpc_release(v);
}

void
xpc_dictionary_set_string(xpc_object_t xdict, const char *key, const char *value)
{
	xpc_object_t v = xpc_string_create(value);
	xpc_dictionary_set_value(xdict, key, v);
	xpc_release(v);
}

void
xpc_dictionary_set_data(xpc_object_t xdict, const char *key, const void *bytes, size_t length)
{
	xpc_object_t v = xpc_data_create(bytes, length);
	xpc_dictionary_set_value(xdict, key, v);
	xpc_release(v);
}

bool
xpc_dictionary_get_bool(xpc_object_t xdict, const char *key)
{
	xpc_object_t v = xpc_dictionary_get_value(xdict, key);
	return v ? xpc_bool_get_value(v) : false;
}

int64_t
xpc_dictionary_get_int64(xpc_object_t xdict, const char *key)
{
	xpc_object_t v = xpc_dictionary_get_value(xdict, key);
	return v ? xpc_int64_get_value(v) : 0;
}

uint64_t
xpc_dictionary_get_uint64(xpc_object_t xdict, const char *key)
{
	xpc_object_t v = xpc_dictionary_get_value(xdict, key);
	return v ? xpc_uint64_get_value(v) : 0;
}

double
xpc_dictionary_get_double(xpc_object_t xdict, const char *key)
{
	xpc_object_t v = xpc_dictionary_get_value(xdict, key);
	return v ? xpc_double_get_value(v) : 0.0;
}

const char *
xpc_dictionary_get_string(xpc_object_t xdict, const char *key)
{
	xpc_object_t v = xpc_dictionary_get_value(xdict, key);
	return v ? xpc_string_get_string_ptr(v) : NULL;
}

const void *
xpc_dictionary_get_data(xpc_object_t xdict, const char *key, size_t *length)
{
	xpc_object_t v = xpc_dictionary_get_value(xdict, key);
	if (!v) {
		*length = 0;
		return NULL;
	}
	*length = xpc_data_get_length(v);
	return xpc_data_get_bytes_ptr(v);
}

xpc_object_t
xpc_dictionary_get_array(xpc_object_t xdict, const char *key)
{
	xpc_object_t v = xpc_dictionary_get_value(xdict, key);
	return (v && xpc_get_type(v) == XPC_TYPE_ARRAY) ? v : NULL;
}

xpc_object_t
xpc_dictionary_get_dictionary(xpc_object_t xdict, const char *key)
{
	xpc_object_t v = xpc_dictionary_get_value(xdict, key);
	return (v && xpc_get_type(v) == XPC_TYPE_DICTIONARY) ? v : NULL;
}

void
_xpc_dictionary_destroy(xpc_object_t obj)
{
	struct xpc_kv *kv = obj->u.dict.head;
	while (kv) {
		struct xpc_kv *next = kv->next;
		free(kv->key);
		xpc_release(kv->value);
		free(kv);
		kv = next;
	}
	if (obj->u.dict.assoc_conn) {
		xpc_release(obj->u.dict.assoc_conn);
	}
}

xpc_object_t
_xpc_dictionary_copy(xpc_object_t obj)
{
	xpc_object_t copy = xpc_dictionary_create_empty();
	/* Insertion order gets reversed by the linked-list-head-insert
	 * copy below (same way the original was built) -- harmless, this
	 * dictionary has no defined order. */
	for (struct xpc_kv *kv = obj->u.dict.head; kv; kv = kv->next) {
		xpc_object_t v = xpc_copy(kv->value);
		xpc_dictionary_set_value(copy, kv->key, v);
		xpc_release(v);
	}
	return copy;
}

bool
_xpc_dictionary_equal(xpc_object_t a, xpc_object_t b)
{
	if (a->u.dict.count != b->u.dict.count) {
		return false;
	}
	for (struct xpc_kv *kv = a->u.dict.head; kv; kv = kv->next) {
		xpc_object_t bv = xpc_dictionary_get_value(b, kv->key);
		if (!bv || !xpc_equal(kv->value, bv)) {
			return false;
		}
	}
	return true;
}
