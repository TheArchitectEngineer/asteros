/* Copyright (c) 2026 Vihaan Nathan
 *
 * Private structs every xpc_object_t and xpc_connection_t is built on.
 * Not part of the public API, same "base struct is a plain refcounted
 * heap object, touched only via __atomic_* builtins" idiom
 * userland/libdispatch/dispatch_internal.h already established for this
 * tree -- xpc_object_s doesn't reuse dispatch_object_hdr itself (this is
 * a separate library with its own type tag, not a dispatch object), but
 * follows the same shape on purpose.
 */
#ifndef XPC_INTERNAL_H
#define XPC_INTERNAL_H

#include <xpc/xpc.h>
#include <mach/bootstrap.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/mach_traps.h>
#include <mach/message.h>
#include <mach/task_special_ports.h>
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>

/* ---- object tags (internal dispatch key, matches _xpc_type_s.xt_tag) ---- */
enum {
	_XPC_TAG_NULL = 1,
	_XPC_TAG_BOOL,
	_XPC_TAG_INT64,
	_XPC_TAG_UINT64,
	_XPC_TAG_DOUBLE,
	_XPC_TAG_STRING,
	_XPC_TAG_DATA,
	_XPC_TAG_ARRAY,
	_XPC_TAG_DICTIONARY,
	_XPC_TAG_CONNECTION,
	_XPC_TAG_ERROR,
};

struct xpc_kv {
	char *key;
	xpc_object_t value;
	struct xpc_kv *next;
};

struct xpc_connection_data;

struct xpc_object_s {
	volatile int refcnt;
	xpc_type_t type;
	union {
		bool b;
		int64_t i64;
		uint64_t u64;
		double d;
		struct { char *bytes; size_t len; } str;   /* NUL-terminated, len excludes the NUL */
		struct { uint8_t *bytes; size_t len; } data;
		struct { xpc_object_t *items; size_t count; size_t cap; } arr;
		struct {
			struct xpc_kv *head;
			size_t count;
			/* stamped only on a dictionary decoded off the wire as a
			 * connection event -- lets xpc_dictionary_create_reply()/
			 * xpc_dictionary_get_remote_connection() work. assoc_conn
			 * holds a retained reference, released when this dictionary
			 * is destroyed. */
			xpc_connection_t assoc_conn;
			uint64_t assoc_msgid;
		} dict;
		struct xpc_connection_data *conn;  /* _XPC_TAG_CONNECTION only */
		const char *err_desc;               /* _XPC_TAG_ERROR only */
	} u;
};

/* ---- connection internals ---- */

struct xpc_pending {
	uint64_t msgid;
	dispatch_semaphore_t sem;      /* sync path: signaled once ->result is filled in */
	xpc_object_t result;
	xpc_handler_t reply_block;     /* async path: Block_copy'd */
	dispatch_queue_t replyq;
	struct xpc_pending *next;
};

struct xpc_peer_entry {
	mach_port_name_t remote_port;
	xpc_connection_t conn;
	struct xpc_peer_entry *next;
};

struct xpc_connection_data {
	mach_port_name_t local_port;   /* our receive right (shared with every accepted peer, if we're a listener) */
	mach_port_name_t remote_port;  /* send right to the peer; MACH_PORT_NULL on a not-yet-activated listener */
	char service_name[BOOTSTRAP_MAX_NAME_LEN]; /* the name passed to xpc_connection_create_mach_service() */
	dispatch_queue_t targetq;
	xpc_handler_t handler;         /* Block_copy'd */
	xpc_handler_f_t handler_f;
	void *handler_f_ctx;
	bool is_listener;              /* owns local_port + the receive thread, demuxes peers */
	bool is_peer;                  /* an accepted peer of a listener -- shares the listener's local_port/thread */
	bool activated;
	bool cancelled;
	volatile int suspend_count;
	pthread_mutex_t lock;          /* guards pending/peers/suspend_count/resume_cond */
	pthread_cond_t resume_cond;
	pthread_t recv_thread;
	bool have_recv_thread;
	uint64_t next_msgid;
	struct xpc_pending *pending;
	struct xpc_peer_entry *peers;  /* listener only */
	void *context;
	xpc_finalizer_t finalizer;
};

/* xpc_object.c */
xpc_object_t _xpc_object_alloc(xpc_type_t type);
xpc_object_t _xpc_string_create_len(const char *bytes, size_t len);
xpc_connection_t _xpc_connection_alloc(void);

/* xpc_serialize.c -- our own recursive TLV format, not Apple's bplist15.
 * encode fails (returns false) past XPC_WIRE_MAX_PAYLOAD; decode fails
 * (returns NULL) on truncated/malformed input -- this is real boundary
 * validation, the peer is a different process. */
#define XPC_WIRE_MAX_PAYLOAD 8192u

bool xpc_serialize(xpc_object_t obj, uint8_t *buf, size_t cap, size_t *out_len);
xpc_object_t xpc_deserialize(const uint8_t *buf, size_t len);

/* ---- wire message shape ----
 * One inline mach_msg per XPC message, no OOL descriptors (see xpc.h's
 * scope note). msgh_local_port carries a MAKE_SEND (not MAKE_SEND_ONCE --
 * we want a durable send right so the peer can keep talking back, not a
 * one-shot reply) disposition; the destination learns/refreshes our
 * address from msgh_remote_port on receipt, same field-swap-on-delivery
 * behavior userland/mach_test/machtest_main.c ground-truthed live.
 * trailer_pad is never sent, only ever used as rcv_size headroom for the
 * kernel-appended receive trailer -- same MAX_TRAILER_SIZE padding
 * convention as machtest/mach_special_ports.c. */
#define XPC_WIRE_MSGH_ID 0x58504331 /* 'XPC1' */

struct xpc_wire_msg {
	mach_msg_header_t header;
	uint64_t msg_id;        /* 0 == fire-and-forget event; nonzero correlates a reply */
	uint8_t is_reply;
	uint8_t _reserved[3];
	uint32_t payload_len;
	uint8_t payload[XPC_WIRE_MAX_PAYLOAD];
	uint8_t trailer_pad[128];
};

/* Rounded up to a multiple of 4: ipc_kmsg_get() (osfmk/ipc/ipc_kmsg.c)
 * rejects any send size that isn't long-word aligned with
 * MACH_SEND_MSG_TOO_SMALL ("Message size not long-word multiple", not
 * actually about smallness -- ground-truthed live after a real
 * xpc_connection_send_message() of an odd-length payload failed with
 * exactly that code). Payload length itself -- what the receiver
 * actually decodes -- is carried explicitly in payload_len, so the up-to-
 * 3 padding bytes this adds are never read by anything; there's always
 * room for them since trailer_pad follows payload in this same struct. */
#define XPC_WIRE_SEND_SIZE(len) \
	((mach_msg_size_t)((offsetof(struct xpc_wire_msg, payload) + (len) + 3u) & ~3u))

#endif /* XPC_INTERNAL_H */
