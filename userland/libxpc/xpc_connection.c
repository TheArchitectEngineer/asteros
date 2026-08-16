/* Copyright (c) 2026 Vihaan Nathan
 *
 * xpc_connection_t: real Mach IPC (userland/libc/src/mach_msg.c) driven by
 * a dedicated receive pthread per listener/client, real async delivery via
 * dispatch_async_f() onto the connection's target queue -- see
 * xpc/connection.h's file header for the design and userland/mach_test/
 * machtest_main.c for the ground-truthed Mach mechanics this reuses
 * (bootstrap-port handoff, receive-buffer trailer padding, the
 * sender/receiver reply-port field swap).
 *
 * Bootstrap: a listener allocates a receive right, mints a send right at
 * the same name, and publishes it under xpc_connection_create_mach_service()'s
 * `name` argument via bootstrap_register() (mach/bootstrap.h) against
 * launchd's real bootstrap-namespace registry (userland/launchd/
 * bootstrap_server.c) -- a client reaches the same port by name via
 * bootstrap_look_up(), not by reading TASK_BOOTSTRAP_PORT directly. Both
 * calls go through TASK_BOOTSTRAP_PORT only to *reach* launchd's registry
 * (every process inherits a send right to it via the kernel's
 * ipc_task_init(), exactly machtest's original single-service mechanism,
 * just now pointed at a real multi-service registry instead of at an
 * individual daemon's own port).
 *
 * Full-duplex addressing: xpc_connection_activate() mints ourselves a
 * self-held send right to our own local_port once (mach_port_insert_right,
 * same "one name, both a receive and a send right" shape the listener's
 * bootstrap-port setup already uses), and every message's Mach header
 * carries that right in msgh_local_port via COPY_SEND -- not a fresh
 * MAKE_SEND derived from the receive right on every send, which would
 * repeat an operation (re-deriving a send right from a receive right,
 * from the same name, many times across one connection's lifetime) this
 * project had otherwise only ever done once per port
 * (userland/mach_test/machtest_main.c, mach_special_ports.c both mint
 * exactly one send right and either consume or discard it -- never a
 * second one from the same receive right later). The kernel delivers our
 * copied right to the recipient as msgh_remote_port on receipt. Because
 * it's the same underlying port every time, the kernel coalesces repeat
 * deliveries to the same recipient into the same port name (real Mach
 * right semantics: a task receiving another send right to a port it
 * already holds any right to gets back the same name, just a bumped user
 * refcount) -- so once a peer's remote_port name is known, a repeat
 * receive with the same name has an extra reference dropped via
 * mach_port_deallocate() rather than leaking (see find_or_create_peer()).
 *
 * Request/reply correlation uses an explicit msg_id (xpc_wire_msg::msg_id)
 * rather than a fresh one-shot reply port per call, since a connection
 * here is a durable two-port channel, not a single MIG-style round trip.
 */
#include "xpc_internal.h"
#include <Block.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void
_xpc_connection_destroy(struct xpc_connection_data *cd)
{
	if (cd->handler) {
		Block_release(cd->handler);
	}
	pthread_mutex_destroy(&cd->lock);
	pthread_cond_destroy(&cd->resume_cond);
	while (cd->pending) {
		struct xpc_pending *next = cd->pending->next;
		if (cd->pending->reply_block) {
			Block_release(cd->pending->reply_block);
		}
		free(cd->pending);
		cd->pending = next;
	}
	while (cd->peers) {
		struct xpc_peer_entry *next = cd->peers->next;
		xpc_release(cd->peers->conn);
		free(cd->peers);
		cd->peers = next;
	}
}

xpc_connection_t
xpc_connection_create_mach_service(const char *name, dispatch_queue_t targetq, uint64_t flags)
{
	xpc_connection_t conn = _xpc_connection_alloc();
	struct xpc_connection_data *cd = conn->u.conn;
	if (name) {
		strncpy(cd->service_name, name, sizeof(cd->service_name) - 1);
		cd->service_name[sizeof(cd->service_name) - 1] = '\0';
	}
	cd->targetq = targetq ? targetq : dispatch_get_main_queue();
	cd->is_listener = (flags & XPC_CONNECTION_MACH_SERVICE_LISTENER) != 0;
	return conn;
}

void
xpc_connection_set_target_queue(xpc_connection_t connection, dispatch_queue_t targetq)
{
	connection->u.conn->targetq = targetq ? targetq : dispatch_get_main_queue();
}

void
xpc_connection_set_event_handler(xpc_connection_t connection, xpc_handler_t handler)
{
	struct xpc_connection_data *cd = connection->u.conn;
	if (cd->handler) {
		Block_release(cd->handler);
	}
	cd->handler = Block_copy(handler);
	cd->handler_f = NULL;
}

void
xpc_connection_set_event_handler_f(xpc_connection_t connection, void *context, xpc_handler_f_t handler)
{
	struct xpc_connection_data *cd = connection->u.conn;
	if (cd->handler) {
		Block_release(cd->handler);
		cd->handler = NULL;
	}
	cd->handler_f = handler;
	cd->handler_f_ctx = context;
}

void
xpc_connection_set_context(xpc_connection_t connection, void *context)
{
	connection->u.conn->context = context;
}

void *
xpc_connection_get_context(xpc_connection_t connection)
{
	return connection->u.conn->context;
}

void
xpc_connection_set_finalizer_f(xpc_connection_t connection, xpc_finalizer_t finalizer)
{
	connection->u.conn->finalizer = finalizer;
}

/* ---- delivery ---- */

struct deliver_ctx {
	xpc_connection_t conn;  /* retained for the lifetime of the dispatch */
	xpc_object_t event;
};

static void
deliver_trampoline(void *context)
{
	struct deliver_ctx *ctx = context;
	struct xpc_connection_data *cd = ctx->conn->u.conn;
	if (cd->handler) {
		cd->handler(ctx->event);
	} else if (cd->handler_f) {
		cd->handler_f(cd->handler_f_ctx, ctx->event);
	}
	xpc_release(ctx->event);
	xpc_release(ctx->conn);
	free(ctx);
}

/* Blocks (gated on cd->suspend_count) until delivery is safe, then hands
 * `event` (a reference this call takes ownership of) to the connection's
 * handler on its target queue. */
static void
deliver_event(xpc_connection_t conn, xpc_object_t event)
{
	struct xpc_connection_data *cd = conn->u.conn;

	pthread_mutex_lock(&cd->lock);
	while (cd->suspend_count > 0 && !cd->cancelled) {
		pthread_cond_wait(&cd->resume_cond, &cd->lock);
	}
	bool cancelled = cd->cancelled;
	pthread_mutex_unlock(&cd->lock);

	if (cancelled || (!cd->handler && !cd->handler_f)) {
		xpc_release(event);
		return;
	}

	struct deliver_ctx *ctx = malloc(sizeof(*ctx));
	ctx->conn = xpc_retain(conn);
	ctx->event = event;
	dispatch_async_f(cd->targetq, ctx, deliver_trampoline);
}

/* dispatch_sync_f trampoline for the one synchronous "accept a new peer"
 * notification -- see the file header on why this one is synchronous
 * (real XPC guarantees a peer's handler is set before it can receive
 * events; our demux loop enforces the same ordering by not moving on to
 * the triggering message until the listener's handler call returns). */
static void
accept_trampoline(void *context)
{
	struct deliver_ctx *ctx = context;
	struct xpc_connection_data *cd = ctx->conn->u.conn;
	if (cd->handler) {
		cd->handler(ctx->event);
	} else if (cd->handler_f) {
		cd->handler_f(cd->handler_f_ctx, ctx->event);
	}
	/* ctx->event (the peer, as an xpc_object_t) keeps its one reference
	 * owned by the listener's cd->peers list -- not released here. */
}

struct reply_ctx {
	xpc_connection_t conn;
	xpc_object_t event;
	xpc_handler_t block;
};

static void
reply_trampoline(void *context)
{
	struct reply_ctx *rc = context;
	rc->block(rc->event);
	Block_release(rc->block);
	xpc_release(rc->event);
	xpc_release(rc->conn);
	free(rc);
}

static void
complete_pending(xpc_connection_t conn, uint64_t msgid, xpc_object_t result)
{
	struct xpc_connection_data *cd = conn->u.conn;
	pthread_mutex_lock(&cd->lock);
	struct xpc_pending **link = &cd->pending;
	struct xpc_pending *p = NULL;
	while (*link) {
		if ((*link)->msgid == msgid) {
			p = *link;
			*link = p->next;
			break;
		}
		link = &(*link)->next;
	}
	pthread_mutex_unlock(&cd->lock);

	if (!p) {
		xpc_release(result);
		return;
	}

	if (p->sem) {
		p->result = result;
		dispatch_semaphore_signal(p->sem);
		/* p itself is freed by the waiter in
		 * xpc_connection_send_message_with_reply_sync once it wakes up. */
	} else {
		struct reply_ctx *rc = malloc(sizeof(*rc));
		rc->conn = xpc_retain(conn);
		rc->event = result;
		rc->block = p->reply_block;
		dispatch_queue_t q = p->replyq;
		free(p);
		dispatch_async_f(q, rc, reply_trampoline);
	}
}

/* ---- receive-side routing ---- */

static xpc_connection_t
find_or_create_peer(xpc_connection_t listener, mach_port_name_t remote_name)
{
	struct xpc_connection_data *lcd = listener->u.conn;

	pthread_mutex_lock(&lcd->lock);
	for (struct xpc_peer_entry *pe = lcd->peers; pe; pe = pe->next) {
		if (pe->remote_port == remote_name) {
			pthread_mutex_unlock(&lcd->lock);
			/* We already hold a right to this peer's port -- the kernel
			 * coalesced this delivery's MAKE_SEND onto the same name and
			 * bumped its refcount; drop the extra reference. */
			mach_port_deallocate(mach_task_self(), remote_name);
			return pe->conn;
		}
	}

	xpc_connection_t peer = _xpc_connection_alloc();
	struct xpc_connection_data *pcd = peer->u.conn;
	pcd->local_port = lcd->local_port;
	pcd->remote_port = remote_name;
	pcd->targetq = lcd->targetq;
	pcd->is_peer = true;
	pcd->activated = true;

	struct xpc_peer_entry *pe = malloc(sizeof(*pe));
	pe->remote_port = remote_name;
	pe->conn = xpc_retain(peer);
	pe->next = lcd->peers;
	lcd->peers = pe;
	pthread_mutex_unlock(&lcd->lock);

	if (lcd->handler || lcd->handler_f) {
		/* accept_trampoline itself branches on handler vs. handler_f --
		 * one dispatch_sync_f covers both listener styles. */
		struct deliver_ctx actx = { listener, peer };
		dispatch_sync_f(lcd->targetq, &actx, accept_trampoline);
	}

	xpc_release(peer); /* cd->peers list holds the long-lived reference */
	return peer;
}

static void
route_incoming(xpc_connection_t conn, struct xpc_wire_msg *msg)
{
	if (msg->is_reply) {
		xpc_object_t result = xpc_deserialize(msg->payload, msg->payload_len);
		if (!result) {
			result = xpc_null_create();
		}
		complete_pending(conn, msg->msg_id, result);
		return;
	}

	xpc_object_t event = xpc_deserialize(msg->payload, msg->payload_len);
	if (!event) {
		event = xpc_null_create();
	}
	if (xpc_get_type(event) == XPC_TYPE_DICTIONARY) {
		/* assoc_conn is stamped for every received dictionary (so
		 * xpc_dictionary_get_remote_connection() always works, matching
		 * real XPC) -- assoc_msgid is 0 for a fire-and-forget event,
		 * which xpc_dictionary_create_reply() takes as "not reply-able",
		 * same as real XPC refusing to reply to a one-way message. */
		event->u.dict.assoc_conn = xpc_retain(conn);
		event->u.dict.assoc_msgid = msg->msg_id;
	}
	deliver_event(conn, event);
}

/* Completes every outstanding xpc_connection_send_message_with_reply(_sync)
 * on `conn` with XPC_ERROR_CONNECTION_INVALID, so a peer that will never
 * reply (its receive thread just died) can't leave a caller blocked
 * forever in dispatch_semaphore_wait(). */
static void
fail_all_pending(xpc_connection_t conn)
{
	struct xpc_connection_data *cd = conn->u.conn;
	pthread_mutex_lock(&cd->lock);
	struct xpc_pending *p = cd->pending;
	cd->pending = NULL;
	pthread_mutex_unlock(&cd->lock);

	while (p) {
		struct xpc_pending *next = p->next;
		if (p->sem) {
			p->result = xpc_retain(XPC_ERROR_CONNECTION_INVALID);
			dispatch_semaphore_signal(p->sem);
			free(p);
		} else {
			struct reply_ctx *rc = malloc(sizeof(*rc));
			rc->conn = xpc_retain(conn);
			rc->event = xpc_retain(XPC_ERROR_CONNECTION_INVALID);
			rc->block = p->reply_block;
			dispatch_queue_t q = p->replyq;
			free(p);
			dispatch_async_f(q, rc, reply_trampoline);
		}
		p = next;
	}
}

static void *
recv_thread_main(void *arg)
{
	xpc_connection_t self = arg;
	struct xpc_connection_data *cd = self->u.conn;

	for (;;) {
		struct xpc_wire_msg msg;
		kern_return_t kr = (kern_return_t)mach_msg(&msg.header, MACH_RCV_MSG, 0,
		    (mach_msg_size_t)sizeof(msg), cd->local_port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
		if (kr != KERN_SUCCESS) {
			break; /* port destroyed by xpc_connection_cancel(), or a real error -- either way, stop */
		}

		xpc_connection_t peer = cd->is_listener
		    ? find_or_create_peer(self, msg.header.msgh_remote_port)
		    : self;
		route_incoming(peer, &msg);
	}

	pthread_mutex_lock(&cd->lock);
	cd->cancelled = true;
	pthread_cond_broadcast(&cd->resume_cond);
	struct xpc_peer_entry *peers = cd->peers;
	pthread_mutex_unlock(&cd->lock);

	fail_all_pending(self);
	for (struct xpc_peer_entry *pe = peers; pe; pe = pe->next) {
		fail_all_pending(pe->conn);
	}

	if (cd->handler || cd->handler_f) {
		deliver_event(self, xpc_retain(XPC_ERROR_CONNECTION_INVALID));
	}
	return NULL;
}

void
xpc_connection_activate(xpc_connection_t connection)
{
	struct xpc_connection_data *cd = connection->u.conn;
	if (cd->activated) {
		return;
	}
	cd->activated = true;

	if (cd->is_peer) {
		return; /* already wired up by find_or_create_peer() */
	}

	kern_return_t kr;
	if (cd->is_listener) {
		mach_port_name_t recv_name;
		kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &recv_name);
		if (kr != KERN_SUCCESS) {
			fprintf(stderr, "xpc: listener mach_port_allocate failed kr=%d\n", kr);
			return;
		}
		/* Mint a send right to publish via bootstrap_register() --
		 * COPY_SEND there leaves this one intact, so it doubles as the
		 * "self-held" send right send_wire() COPY_SENDs on every
		 * outgoing message (see its own comment for why that's COPY_SEND
		 * rather than a fresh MAKE_SEND per send). */
		kr = mach_port_insert_right(mach_task_self(), recv_name, recv_name, MACH_MSG_TYPE_MAKE_SEND);
		if (kr != KERN_SUCCESS) {
			fprintf(stderr, "xpc: listener mach_port_insert_right failed kr=%d\n", kr);
			return;
		}
		cd->local_port = recv_name;

		/* Publish under our own name against launchd's registry instead
		 * of clobbering our own TASK_BOOTSTRAP_PORT -- that port is now
		 * launchd's shared registry port, inherited from launchd at
		 * fork time, and every other daemon needs it left alone. */
		mach_port_t bp = MACH_PORT_NULL;
		kr = task_get_special_port(mach_task_self(), TASK_BOOTSTRAP_PORT, &bp);
		if (kr != KERN_SUCCESS) {
			fprintf(stderr, "xpc: listener task_get_special_port failed kr=%d\n", kr);
			return;
		}
		kr = bootstrap_register(bp, cd->service_name, (mach_port_t)recv_name);
		mach_port_deallocate(mach_task_self(), bp);
		if (kr != KERN_SUCCESS) {
			fprintf(stderr, "xpc: listener bootstrap_register(\"%s\") failed kr=%d\n", cd->service_name, kr);
			return;
		}
	} else {
		mach_port_t bp = MACH_PORT_NULL;
		kr = task_get_special_port(mach_task_self(), TASK_BOOTSTRAP_PORT, &bp);
		if (kr != KERN_SUCCESS) {
			fprintf(stderr, "xpc: client task_get_special_port failed kr=%d\n", kr);
			return;
		}
		mach_port_t send_port = MACH_PORT_NULL;
		kr = bootstrap_look_up(bp, cd->service_name, &send_port);
		mach_port_deallocate(mach_task_self(), bp);
		if (kr != KERN_SUCCESS) {
			fprintf(stderr, "xpc: client bootstrap_look_up(\"%s\") failed kr=%d\n", cd->service_name, kr);
			return;
		}
		cd->remote_port = send_port;

		mach_port_name_t recv_name;
		kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &recv_name);
		if (kr != KERN_SUCCESS) {
			fprintf(stderr, "xpc: client mach_port_allocate failed kr=%d\n", kr);
			return;
		}
		/* Same self-held send right as the listener branch above. */
		kr = mach_port_insert_right(mach_task_self(), recv_name, recv_name, MACH_MSG_TYPE_MAKE_SEND);
		if (kr != KERN_SUCCESS) {
			fprintf(stderr, "xpc: client self mach_port_insert_right failed kr=%d\n", kr);
			return;
		}
		cd->local_port = recv_name;
	}

	xpc_retain(connection); /* the receive thread holds a reference to its own connection object */
	pthread_create(&cd->recv_thread, NULL, recv_thread_main, connection);
	cd->have_recv_thread = true;
}

void
xpc_connection_resume(xpc_connection_t connection)
{
	struct xpc_connection_data *cd = connection->u.conn;
	xpc_connection_activate(connection);
	pthread_mutex_lock(&cd->lock);
	if (cd->suspend_count > 0) {
		cd->suspend_count--;
	}
	if (cd->suspend_count == 0) {
		pthread_cond_broadcast(&cd->resume_cond);
	}
	pthread_mutex_unlock(&cd->lock);
}

void
xpc_connection_suspend(xpc_connection_t connection)
{
	struct xpc_connection_data *cd = connection->u.conn;
	pthread_mutex_lock(&cd->lock);
	cd->suspend_count++;
	pthread_mutex_unlock(&cd->lock);
}

void
xpc_connection_cancel(xpc_connection_t connection)
{
	struct xpc_connection_data *cd = connection->u.conn;
	pthread_mutex_lock(&cd->lock);
	cd->cancelled = true;
	pthread_cond_broadcast(&cd->resume_cond);
	pthread_mutex_unlock(&cd->lock);

	if (!cd->is_peer && cd->local_port != MACH_PORT_NULL) {
		/* Destroying our own receive right is what actually unblocks the
		 * recv thread's mach_msg(MACH_RCV_MSG) -- a flag alone can't,
		 * it's parked in the kernel. */
		mach_port_destroy(mach_task_self(), cd->local_port);
	}
}

/* ---- sending ---- */

static void
send_wire(struct xpc_connection_data *cd, uint64_t msgid, bool is_reply, xpc_object_t message)
{
	struct xpc_wire_msg msg;
	size_t len = 0;
	if (!xpc_serialize(message, msg.payload, XPC_WIRE_MAX_PAYLOAD, &len)) {
		fprintf(stderr, "xpc: message too large to serialize (cap %u bytes)\n", XPC_WIRE_MAX_PAYLOAD);
		return;
	}

	/* Both fields COPY_SEND: cd->remote_port from a right we hold to the
	 * peer, cd->local_port from the self-held send right minted once in
	 * xpc_connection_activate() (same name as our receive right -- one
	 * name can hold both). Deliberately not MAKE_SEND on the local
	 * field: that would re-derive a *new* send right from the receive
	 * right on every single send, repeatedly exercising a path
	 * (mach_msg_trap's userland copyin of msgh_local_port's disposition,
	 * with COMPLEX un-set and the port a locally-held receive right)
	 * this project has only ever used once per port (see
	 * userland/mach_test/machtest_main.c, mach_special_ports.c) -- never
	 * repeatedly, from the same name, across many sends on one
	 * connection's lifetime. COPY_SEND of an already-minted right is the
	 * same well-exercised operation already used for the remote field. */
	msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_COPY_SEND);
	msg.header.msgh_size = XPC_WIRE_SEND_SIZE(len);
	msg.header.msgh_remote_port = (mach_port_t)cd->remote_port;
	msg.header.msgh_local_port = (mach_port_t)cd->local_port;
	msg.header.msgh_voucher_port = MACH_PORT_NULL;
	msg.header.msgh_id = XPC_WIRE_MSGH_ID;
	msg.msg_id = msgid;
	msg.is_reply = is_reply ? 1 : 0;
	msg.payload_len = (uint32_t)len;

	kern_return_t kr = (kern_return_t)mach_msg(&msg.header, MACH_SEND_MSG, XPC_WIRE_SEND_SIZE(len),
	    0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "xpc: send failed kr=%d\n", kr);
	}
}

void
xpc_connection_send_message(xpc_connection_t connection, xpc_object_t message)
{
	struct xpc_connection_data *cd = connection->u.conn;
	uint64_t msgid = 0;
	bool is_reply = false;
	if (xpc_get_type(message) == XPC_TYPE_DICTIONARY && message->u.dict.assoc_msgid != 0) {
		msgid = message->u.dict.assoc_msgid;
		is_reply = true;
	}
	send_wire(cd, msgid, is_reply, message);
}

void
xpc_connection_send_message_with_reply(xpc_connection_t connection, xpc_object_t message,
    dispatch_queue_t replyq, xpc_handler_t handler)
{
	struct xpc_connection_data *cd = connection->u.conn;

	struct xpc_pending *p = malloc(sizeof(*p));
	pthread_mutex_lock(&cd->lock);
	p->msgid = ++cd->next_msgid;
	pthread_mutex_unlock(&cd->lock);
	p->sem = NULL;
	p->result = NULL;
	p->reply_block = Block_copy(handler);
	p->replyq = replyq ? replyq : cd->targetq;

	pthread_mutex_lock(&cd->lock);
	p->next = cd->pending;
	cd->pending = p;
	pthread_mutex_unlock(&cd->lock);

	send_wire(cd, p->msgid, false, message);
}

xpc_object_t
xpc_connection_send_message_with_reply_sync(xpc_connection_t connection, xpc_object_t message)
{
	struct xpc_connection_data *cd = connection->u.conn;

	struct xpc_pending *p = malloc(sizeof(*p));
	pthread_mutex_lock(&cd->lock);
	p->msgid = ++cd->next_msgid;
	pthread_mutex_unlock(&cd->lock);
	p->sem = dispatch_semaphore_create(0);
	p->result = NULL;
	p->reply_block = NULL;
	p->replyq = NULL;

	pthread_mutex_lock(&cd->lock);
	p->next = cd->pending;
	cd->pending = p;
	pthread_mutex_unlock(&cd->lock);

	send_wire(cd, p->msgid, false, message);

	dispatch_semaphore_wait(p->sem, DISPATCH_TIME_FOREVER);
	dispatch_release(p->sem);
	xpc_object_t result = p->result;
	free(p);
	return result;
}
