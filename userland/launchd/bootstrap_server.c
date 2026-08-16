/* Copyright (c) 2026 Vihaan Nathan
 *
 * launchd's real bootstrap-namespace registry: the server half of
 * mach/bootstrap.h's bootstrap_register()/bootstrap_look_up() protocol
 * (client half: userland/libc/src/mach_bootstrap.c). Replaces the old
 * "one implicit service per process tree" hack, where a daemon installed
 * its own receive right as TASK_BOOTSTRAP_PORT directly and a client just
 * read that same special port back -- see userland/libxpc/xpc_connection.c.
 *
 * bootstrap_server_start() installs launchd's own receive right as its
 * own TASK_BOOTSTRAP_PORT (exactly the "receiver mints its own send
 * right, installs it as TASK_BOOTSTRAP_PORT" idiom
 * userland/mach_test/machtest_main.c already ground-truthed for a single
 * service -- applied here at the top of the whole process tree instead
 * of ad hoc per daemon). Every process launchd forks afterward inherits a
 * send right to this port automatically via the kernel's
 * ipc_task_init() parent-copy -- no explicit handoff needed, same as
 * every earlier phase relying on this mechanism.
 *
 * The registry itself is a plain mutex-guarded singly-linked list --
 * same "simple over premature" tradeoff as dispatch_queue.c's runnable
 * list and libxpc's own dictionary; there is no reason to reach for
 * anything richer at the scale of "the daemons this OS actually boots."
 *
 * Reply construction (msgh_remote_port = the received request's own
 * msgh_remote_port field, disposition MOVE_SEND_ONCE) is ground-truthed
 * against machtest_main.c's own reply code: the kernel swaps the
 * request's msgh_local_port (the sender's reply-to right) into the
 * *receiver's* msgh_remote_port field on delivery -- confirmed live in
 * QEMU there, reused verbatim here.
 *
 * Documented v1 limitations (same "stated up front" discipline as every
 * other phase's known-gaps list):
 *  - No MachServices plist parsing / on-demand (lazy-launch) activation.
 *    A daemon calls bootstrap_register() itself once it's already
 *    running, same as it already calls xpc_connection_create_mach_service()
 *    today.
 *  - No dead-name/no-more-senders notification: a registry entry for a
 *    crashed service isn't pruned automatically. A later lookup still
 *    returns its stale port; the failure only surfaces when a client
 *    actually tries to send to it.
 */
#include "bootstrap_server.h"
#include <mach/bootstrap.h>
#include <mach/bootstrap_priv.h>
#include <mach/kern_return.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/mach_traps.h>
#include <mach/message.h>
#include <mach/ndr.h>
#include <mach/port.h>
#include <mach/task_special_ports.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const NDR_record_t kNDRRecord = {0, 0, 0, 0, 0, 0, 0, 0};

struct bs_service {
	char name[BOOTSTRAP_MAX_NAME_LEN];
	mach_port_name_t port;
	struct bs_service *next;
};

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static struct bs_service *g_services;

static struct bs_service *
find_locked(const char *name)
{
	for (struct bs_service *s = g_services; s; s = s->next) {
		if (strncmp(s->name, name, BOOTSTRAP_MAX_NAME_LEN) == 0) {
			return s;
		}
	}
	return NULL;
}

/* Buffer big enough to receive either request shape -- a plain union of
 * the two real request structs (not a synthetic same-header-different-
 * body cast) so accessing either member is well-defined regardless of
 * which one the kernel actually filled in; msgh_id says which member is
 * live. Trailer headroom is the same MAX_TRAILER_SIZE padding
 * machtest_main.c/mach_special_ports.c already establish is required for
 * every receive. */
union bootstrap_request {
	mach_msg_header_t Head;
	struct bootstrap_register_request reg;
	struct bootstrap_look_up_request lookup;
	char pad[sizeof(struct bootstrap_register_request) + MAX_TRAILER_SIZE];
};

static void
handle_register(union bootstrap_request *req)
{
	char name[BOOTSTRAP_MAX_NAME_LEN];
	strncpy(name, req->reg.name, sizeof(name) - 1);
	name[sizeof(name) - 1] = '\0';
	mach_port_name_t port = req->reg.service_port.name;

	pthread_mutex_lock(&g_lock);
	struct bs_service *s = find_locked(name);
	if (s) {
		/* Replace: a respawned daemon re-registering after a crash.
		 * Drop launchd's own copy of the previous holder's right
		 * first so it doesn't leak. */
		mach_port_deallocate(mach_task_self(), s->port);
		s->port = port;
	} else {
		s = malloc(sizeof(*s));
		strncpy(s->name, name, sizeof(s->name) - 1);
		s->name[sizeof(s->name) - 1] = '\0';
		s->port = port;
		s->next = g_services;
		g_services = s;
	}
	pthread_mutex_unlock(&g_lock);

	struct bootstrap_mig_simple_reply reply;
	reply.Head.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE, 0);
	reply.Head.msgh_size = (mach_msg_size_t)sizeof(reply);
	reply.Head.msgh_remote_port = req->reg.Head.msgh_remote_port; /* the reply-once right -- see file header */
	reply.Head.msgh_local_port = MACH_PORT_NULL;
	reply.Head.msgh_voucher_port = MACH_PORT_NULL;
	reply.Head.msgh_id = BOOTSTRAP_REGISTER_MSGH_ID + 100;
	reply.NDR = kNDRRecord;
	reply.RetCode = KERN_SUCCESS;

	kern_return_t kr = (kern_return_t)mach_msg(&reply.Head, MACH_SEND_MSG,
	    (mach_msg_size_t)sizeof(reply), 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "[launchd] bootstrap: register reply send failed kr=%d\n", kr);
	}
}

static void
handle_look_up(union bootstrap_request *req)
{
	char name[BOOTSTRAP_MAX_NAME_LEN];
	strncpy(name, req->lookup.name, sizeof(name) - 1);
	name[sizeof(name) - 1] = '\0';

	pthread_mutex_lock(&g_lock);
	struct bs_service *s = find_locked(name);
	mach_port_name_t found_port = s ? s->port : MACH_PORT_NULL;
	pthread_mutex_unlock(&g_lock);

	if (s) {
		struct bootstrap_look_up_reply_success reply;
		reply.Head.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE, 0) | MACH_MSGH_BITS_COMPLEX;
		reply.Head.msgh_size = (mach_msg_size_t)sizeof(reply);
		reply.Head.msgh_remote_port = req->lookup.Head.msgh_remote_port;
		reply.Head.msgh_local_port = MACH_PORT_NULL;
		reply.Head.msgh_voucher_port = MACH_PORT_NULL;
		reply.Head.msgh_id = BOOTSTRAP_LOOK_UP_MSGH_ID + 100;
		reply.msgh_body.msgh_descriptor_count = 1;
		/* COPY_SEND: launchd keeps its own stored right so a later
		 * lookup of the same name still works -- same disposition
		 * idiom xpc_connection.c's send_wire() already uses for a
		 * locally-held send right embedded repeatedly. */
		reply.service_port.name = found_port;
		reply.service_port.pad1 = 0;
		reply.service_port.pad2 = 0;
		reply.service_port.disposition = MACH_MSG_TYPE_COPY_SEND;
		reply.service_port.type = MACH_MSG_PORT_DESCRIPTOR;

		kern_return_t kr = (kern_return_t)mach_msg(&reply.Head, MACH_SEND_MSG,
		    (mach_msg_size_t)sizeof(reply), 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
		if (kr != KERN_SUCCESS) {
			fprintf(stderr, "[launchd] bootstrap: look_up reply send failed kr=%d\n", kr);
		}
	} else {
		struct bootstrap_mig_simple_reply reply;
		reply.Head.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE, 0);
		reply.Head.msgh_size = (mach_msg_size_t)sizeof(reply);
		reply.Head.msgh_remote_port = req->lookup.Head.msgh_remote_port;
		reply.Head.msgh_local_port = MACH_PORT_NULL;
		reply.Head.msgh_voucher_port = MACH_PORT_NULL;
		reply.Head.msgh_id = BOOTSTRAP_LOOK_UP_MSGH_ID + 100;
		reply.NDR = kNDRRecord;
		reply.RetCode = KERN_INVALID_NAME;

		kern_return_t kr = (kern_return_t)mach_msg(&reply.Head, MACH_SEND_MSG,
		    (mach_msg_size_t)sizeof(reply), 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
		if (kr != KERN_SUCCESS) {
			fprintf(stderr, "[launchd] bootstrap: look_up error reply send failed kr=%d\n", kr);
		}
	}
}

static mach_port_name_t g_recv_name;

static void *
server_thread_main(void *arg)
{
	(void)arg;
	for (;;) {
		union bootstrap_request req;
		kern_return_t kr = (kern_return_t)mach_msg(&req.Head, MACH_RCV_MSG, 0,
		    (mach_msg_size_t)sizeof(req), g_recv_name, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
		if (kr != KERN_SUCCESS) {
			fprintf(stderr, "[launchd] bootstrap: receive failed kr=%d\n", kr);
			continue;
		}

		if (req.Head.msgh_id == BOOTSTRAP_REGISTER_MSGH_ID) {
			handle_register(&req);
		} else if (req.Head.msgh_id == BOOTSTRAP_LOOK_UP_MSGH_ID) {
			handle_look_up(&req);
		} else {
			fprintf(stderr, "[launchd] bootstrap: unknown msgh_id 0x%x\n", (unsigned)req.Head.msgh_id);
		}
	}
	return NULL;
}

void
bootstrap_server_start(void)
{
	kern_return_t kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &g_recv_name);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "[launchd] bootstrap: mach_port_allocate failed kr=%d\n", kr);
		return;
	}
	kr = mach_port_insert_right(mach_task_self(), g_recv_name, g_recv_name, MACH_MSG_TYPE_MAKE_SEND);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "[launchd] bootstrap: mach_port_insert_right failed kr=%d\n", kr);
		return;
	}
	kr = task_set_special_port(mach_task_self(), TASK_BOOTSTRAP_PORT, g_recv_name);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "[launchd] bootstrap: task_set_special_port failed kr=%d\n", kr);
		return;
	}

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_t t;
	pthread_create(&t, &attr, server_thread_main, NULL);
	pthread_attr_destroy(&attr);
}
