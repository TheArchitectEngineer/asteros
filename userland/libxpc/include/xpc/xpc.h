/* Copyright (c) 2026 Vihaan Nathan
 *
 * v1-scoped libxpc: real xpc_object_t (null/bool/int64/uint64/double/
 * string/data/array/dictionary), a real recursive TLV wire format of our
 * own design (xpc_serialize.c -- not Apple's bplist15 on-wire format,
 * since nothing outside this OS's own process pairs ever needs to read
 * it), and real xpc_connection_t built directly on this tree's Mach IPC
 * (userland/libc/src/mach_msg.c) and libdispatch (userland/libdispatch)
 * -- see xpc/connection.h.
 *
 * Deliberately out of v1 (documented, not oversights -- see TODO.md's
 * libxpc phase for the full reasoning):
 *   - No OOL Mach descriptors. Every message is a single inline mach_msg
 *     send capped at XPC_WIRE_MAX_PAYLOAD (xpc_internal.h) -- userland OOL
 *     send/receive is unwritten anywhere in this tree (kernel-side copyin/
 *     copyout is real and unmodified, but no userland code has ever built
 *     a MACH_MSGH_BITS_COMPLEX message with an OOL descriptor yet).
 *   - No xpc_fd_t/xpc_shmem_t/xpc_uuid_t/xpc_date_t/xpc_endpoint_t --
 *     each needs either OOL/rights plumbing or a service this OS doesn't
 *     have yet.
 *   - No named multi-service bootstrap lookup. Only the same
 *     one-well-known-port-per-process-tree pattern
 *     userland/mach_test/machtest_main.c already established (a listener
 *     installs its receive right as its own TASK_BOOTSTRAP_PORT; children
 *     forked afterward inherit a send right to it automatically). Real
 *     xpc_connection_create_mach_service()'s "look up an arbitrary named
 *     service in a shared namespace" semantics need a bootstrap daemon
 *     this OS doesn't have (that's Phase 25's configd groundwork).
 *   - No code-signing / entitlement peer-requirement checks (no
 *     code-signing subsystem to check against).
 */
#ifndef __XPC_XPC_H__
#define __XPC_XPC_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <dispatch/dispatch.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XPC_EXPORT extern

/* ---- object model ----
 * xpc_connection_t is a transparent alias for xpc_object_t, same as real
 * XPC -- a listener's event handler receives new peers as a plain
 * xpc_object_t of type XPC_TYPE_CONNECTION, no cast needed to treat it as
 * an xpc_connection_t (see xpc/connection.h). */
typedef struct xpc_object_s *xpc_object_t;
typedef struct xpc_object_s *xpc_connection_t;

typedef void (^xpc_handler_t)(xpc_object_t object);
typedef void (*xpc_handler_f_t)(void *context, xpc_object_t object);

struct _xpc_type_s {
	const char *xt_name;
	int xt_tag;
};
typedef const struct _xpc_type_s *xpc_type_t;

XPC_EXPORT const struct _xpc_type_s _xpc_type_null;
XPC_EXPORT const struct _xpc_type_s _xpc_type_bool;
XPC_EXPORT const struct _xpc_type_s _xpc_type_int64;
XPC_EXPORT const struct _xpc_type_s _xpc_type_uint64;
XPC_EXPORT const struct _xpc_type_s _xpc_type_double;
XPC_EXPORT const struct _xpc_type_s _xpc_type_string;
XPC_EXPORT const struct _xpc_type_s _xpc_type_data;
XPC_EXPORT const struct _xpc_type_s _xpc_type_array;
XPC_EXPORT const struct _xpc_type_s _xpc_type_dictionary;
XPC_EXPORT const struct _xpc_type_s _xpc_type_connection;
XPC_EXPORT const struct _xpc_type_s _xpc_type_error;

#define XPC_TYPE_NULL (&_xpc_type_null)
#define XPC_TYPE_BOOL (&_xpc_type_bool)
#define XPC_TYPE_INT64 (&_xpc_type_int64)
#define XPC_TYPE_UINT64 (&_xpc_type_uint64)
#define XPC_TYPE_DOUBLE (&_xpc_type_double)
#define XPC_TYPE_STRING (&_xpc_type_string)
#define XPC_TYPE_DATA (&_xpc_type_data)
#define XPC_TYPE_ARRAY (&_xpc_type_array)
#define XPC_TYPE_DICTIONARY (&_xpc_type_dictionary)
#define XPC_TYPE_CONNECTION (&_xpc_type_connection)
#define XPC_TYPE_ERROR (&_xpc_type_error)

/* Predefined, non-refcounted error singletons -- same contract as real
 * XPC: don't xpc_retain/xpc_release these. */
XPC_EXPORT xpc_object_t const XPC_ERROR_CONNECTION_INTERRUPTED;
XPC_EXPORT xpc_object_t const XPC_ERROR_CONNECTION_INVALID;
XPC_EXPORT xpc_object_t const XPC_ERROR_TERMINATION_IMMINENT;

XPC_EXPORT xpc_object_t xpc_retain(xpc_object_t object);
XPC_EXPORT void xpc_release(xpc_object_t object);
XPC_EXPORT xpc_type_t xpc_get_type(xpc_object_t object);
XPC_EXPORT const char *xpc_type_get_name(xpc_type_t type);
XPC_EXPORT xpc_object_t xpc_copy(xpc_object_t object);
XPC_EXPORT bool xpc_equal(xpc_object_t object1, xpc_object_t object2);
/* Caller owns the returned string, free() it. */
XPC_EXPORT char *xpc_copy_description(xpc_object_t object);

/* ---- scalars ---- */
XPC_EXPORT xpc_object_t xpc_null_create(void);

XPC_EXPORT xpc_object_t xpc_bool_create(bool value);
XPC_EXPORT bool xpc_bool_get_value(xpc_object_t xbool);

XPC_EXPORT xpc_object_t xpc_int64_create(int64_t value);
XPC_EXPORT int64_t xpc_int64_get_value(xpc_object_t xint);

XPC_EXPORT xpc_object_t xpc_uint64_create(uint64_t value);
XPC_EXPORT uint64_t xpc_uint64_get_value(xpc_object_t xuint);

XPC_EXPORT xpc_object_t xpc_double_create(double value);
XPC_EXPORT double xpc_double_get_value(xpc_object_t xdouble);

XPC_EXPORT xpc_object_t xpc_string_create(const char *string);
XPC_EXPORT size_t xpc_string_get_length(xpc_object_t xstring);
XPC_EXPORT const char *xpc_string_get_string_ptr(xpc_object_t xstring);

XPC_EXPORT xpc_object_t xpc_data_create(const void *bytes, size_t length);
XPC_EXPORT size_t xpc_data_get_length(xpc_object_t xdata);
XPC_EXPORT const void *xpc_data_get_bytes_ptr(xpc_object_t xdata);
XPC_EXPORT size_t xpc_data_get_bytes(xpc_object_t xdata, void *buffer, size_t off, size_t length);

/* ---- array ---- */
typedef bool (^xpc_array_applier_t)(size_t index, xpc_object_t value);

#define XPC_ARRAY_APPEND ((size_t)-1)

XPC_EXPORT xpc_object_t xpc_array_create(const xpc_object_t *objects, size_t count);
XPC_EXPORT xpc_object_t xpc_array_create_empty(void);
XPC_EXPORT void xpc_array_set_value(xpc_object_t xarray, size_t index, xpc_object_t value);
XPC_EXPORT void xpc_array_append_value(xpc_object_t xarray, xpc_object_t value);
XPC_EXPORT size_t xpc_array_get_count(xpc_object_t xarray);
XPC_EXPORT xpc_object_t xpc_array_get_value(xpc_object_t xarray, size_t index);
XPC_EXPORT bool xpc_array_apply(xpc_object_t xarray, xpc_array_applier_t applier);

/* ---- dictionary ---- */
typedef bool (^xpc_dictionary_applier_t)(const char *key, xpc_object_t value);

XPC_EXPORT xpc_object_t xpc_dictionary_create(const char * const *keys, const xpc_object_t *values, size_t count);
XPC_EXPORT xpc_object_t xpc_dictionary_create_empty(void);
/* Only valid on a dictionary received as a connection event -- ties the
 * reply to the original request's correlation id, see xpc/connection.h. */
XPC_EXPORT xpc_object_t xpc_dictionary_create_reply(xpc_object_t original);
XPC_EXPORT void xpc_dictionary_set_value(xpc_object_t xdict, const char *key, xpc_object_t value);
XPC_EXPORT xpc_object_t xpc_dictionary_get_value(xpc_object_t xdict, const char *key);
XPC_EXPORT size_t xpc_dictionary_get_count(xpc_object_t xdict);
XPC_EXPORT bool xpc_dictionary_apply(xpc_object_t xdict, xpc_dictionary_applier_t applier);
XPC_EXPORT xpc_connection_t xpc_dictionary_get_remote_connection(xpc_object_t xdict);

XPC_EXPORT void xpc_dictionary_set_bool(xpc_object_t xdict, const char *key, bool value);
XPC_EXPORT void xpc_dictionary_set_int64(xpc_object_t xdict, const char *key, int64_t value);
XPC_EXPORT void xpc_dictionary_set_uint64(xpc_object_t xdict, const char *key, uint64_t value);
XPC_EXPORT void xpc_dictionary_set_double(xpc_object_t xdict, const char *key, double value);
XPC_EXPORT void xpc_dictionary_set_string(xpc_object_t xdict, const char *key, const char *value);
XPC_EXPORT void xpc_dictionary_set_data(xpc_object_t xdict, const char *key, const void *bytes, size_t length);

XPC_EXPORT bool xpc_dictionary_get_bool(xpc_object_t xdict, const char *key);
XPC_EXPORT int64_t xpc_dictionary_get_int64(xpc_object_t xdict, const char *key);
XPC_EXPORT uint64_t xpc_dictionary_get_uint64(xpc_object_t xdict, const char *key);
XPC_EXPORT double xpc_dictionary_get_double(xpc_object_t xdict, const char *key);
XPC_EXPORT const char *xpc_dictionary_get_string(xpc_object_t xdict, const char *key);
XPC_EXPORT const void *xpc_dictionary_get_data(xpc_object_t xdict, const char *key, size_t *length);
XPC_EXPORT xpc_object_t xpc_dictionary_get_array(xpc_object_t xdict, const char *key);
XPC_EXPORT xpc_object_t xpc_dictionary_get_dictionary(xpc_object_t xdict, const char *key);

#ifdef __cplusplus
}
#endif

#include <xpc/connection.h>

#endif /* __XPC_XPC_H__ */
