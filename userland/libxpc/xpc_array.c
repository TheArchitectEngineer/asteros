/* Copyright (c) 2026 Vihaan Nathan
 *
 * xpc_object_t array: a plain growable xpc_object_t* vector. Every stored
 * element is retained on the way in, released on the way out -- same
 * ownership contract as xpc_dictionary.c.
 */
#include "xpc_internal.h"
#include <stdlib.h>
#include <string.h>

xpc_object_t
xpc_array_create(const xpc_object_t *objects, size_t count)
{
	xpc_object_t arr = xpc_array_create_empty();
	for (size_t i = 0; i < count; i++) {
		xpc_array_append_value(arr, objects[i]);
	}
	return arr;
}

xpc_object_t
xpc_array_create_empty(void)
{
	xpc_object_t obj = _xpc_object_alloc(XPC_TYPE_ARRAY);
	obj->u.arr.items = NULL;
	obj->u.arr.count = 0;
	obj->u.arr.cap = 0;
	return obj;
}

static void
ensure_cap(xpc_object_t xarray, size_t need)
{
	if (need <= xarray->u.arr.cap) {
		return;
	}
	size_t newcap = xarray->u.arr.cap ? xarray->u.arr.cap * 2 : 4;
	while (newcap < need) {
		newcap *= 2;
	}
	xarray->u.arr.items = realloc(xarray->u.arr.items, newcap * sizeof(xpc_object_t));
	xarray->u.arr.cap = newcap;
}

void
xpc_array_append_value(xpc_object_t xarray, xpc_object_t value)
{
	ensure_cap(xarray, xarray->u.arr.count + 1);
	xarray->u.arr.items[xarray->u.arr.count++] = xpc_retain(value);
}

void
xpc_array_set_value(xpc_object_t xarray, size_t index, xpc_object_t value)
{
	if (index == XPC_ARRAY_APPEND) {
		xpc_array_append_value(xarray, value);
		return;
	}
	if (index >= xarray->u.arr.count) {
		size_t old_count = xarray->u.arr.count;
		ensure_cap(xarray, index + 1);
		for (size_t i = old_count; i < index; i++) {
			xarray->u.arr.items[i] = xpc_null_create();
		}
		xarray->u.arr.count = index + 1;
	} else {
		xpc_release(xarray->u.arr.items[index]);
	}
	xarray->u.arr.items[index] = xpc_retain(value);
}

size_t
xpc_array_get_count(xpc_object_t xarray)
{
	return xarray->u.arr.count;
}

xpc_object_t
xpc_array_get_value(xpc_object_t xarray, size_t index)
{
	if (index >= xarray->u.arr.count) {
		return NULL;
	}
	return xarray->u.arr.items[index];
}

bool
xpc_array_apply(xpc_object_t xarray, xpc_array_applier_t applier)
{
	for (size_t i = 0; i < xarray->u.arr.count; i++) {
		if (!applier(i, xarray->u.arr.items[i])) {
			return false;
		}
	}
	return true;
}

void
_xpc_array_destroy(xpc_object_t obj)
{
	for (size_t i = 0; i < obj->u.arr.count; i++) {
		xpc_release(obj->u.arr.items[i]);
	}
	free(obj->u.arr.items);
}

xpc_object_t
_xpc_array_copy(xpc_object_t obj)
{
	xpc_object_t copy = xpc_array_create_empty();
	for (size_t i = 0; i < obj->u.arr.count; i++) {
		xpc_object_t v = xpc_copy(obj->u.arr.items[i]);
		xpc_array_append_value(copy, v);
		xpc_release(v);
	}
	return copy;
}

bool
_xpc_array_equal(xpc_object_t a, xpc_object_t b)
{
	if (a->u.arr.count != b->u.arr.count) {
		return false;
	}
	for (size_t i = 0; i < a->u.arr.count; i++) {
		if (!xpc_equal(a->u.arr.items[i], b->u.arr.items[i])) {
			return false;
		}
	}
	return true;
}
