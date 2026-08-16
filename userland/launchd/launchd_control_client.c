/* Copyright (c) 2026 Vihaan Nathan
 *
 * Client stubs for launchd_control.h's protocol -- see that header for
 * the wire shapes and design notes. Hand-marshaled exactly like
 * mach/mach_special_ports.c and mach/mach_bootstrap.c: own NDR record,
 * mach_msg_overwrite() with a separate reply buffer (avoids the same-
 * buffer clobber hazard those files document), reply buffers padded with
 * MAX_TRAILER_SIZE headroom.
 */
#include "launchd_control.h"
#include <mach/bootstrap.h>
#include <mach/kern_return.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/mach_traps.h>
#include <mach/task_special_ports.h>
#include <string.h>

static const NDR_record_t kNDRRecord = {0, 0, 0, 0, 0, 0, 0, 0};

/* Resolves LCTL_SERVICE_NAME against launchd's registry. On success the
 * caller owns *out and must mach_port_deallocate() it. */
static kern_return_t
lctl_get_control_port(mach_port_t *out)
{
	mach_port_t bp = MACH_PORT_NULL;
	kern_return_t kr = task_get_special_port(mach_task_self(), TASK_BOOTSTRAP_PORT, &bp);
	if (kr != KERN_SUCCESS) {
		return kr;
	}
	kr = bootstrap_look_up(bp, LCTL_SERVICE_NAME, out);
	mach_port_deallocate(mach_task_self(), bp);
	return kr;
}

int
lctl_list(struct lctl_job_wire *out, int max)
{
	mach_port_t cp;
	if (lctl_get_control_port(&cp) != KERN_SUCCESS) {
		return -1;
	}

	struct lctl_list_request req;
	union {
		struct lctl_list_reply reply;
		char pad[sizeof(struct lctl_list_reply) + MAX_TRAILER_SIZE];
	} rbuf;
	mach_port_name_t reply_port = mach_reply_port();

	req.Head.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
	req.Head.msgh_size = (mach_msg_size_t)sizeof(req);
	req.Head.msgh_remote_port = cp;
	req.Head.msgh_local_port = (mach_port_t)reply_port;
	req.Head.msgh_voucher_port = MACH_PORT_NULL;
	req.Head.msgh_id = LCTL_LIST_MSGH_ID;
	req.NDR = kNDRRecord;

	mach_msg_return_t mr = mach_msg_overwrite(&req.Head, MACH_SEND_MSG | MACH_RCV_MSG,
	    (mach_msg_size_t)sizeof(req), (mach_msg_size_t)sizeof(rbuf),
	    reply_port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL,
	    &rbuf.reply.Head, (mach_msg_size_t)sizeof(rbuf));

	mach_port_deallocate(mach_task_self(), reply_port);
	mach_port_deallocate(mach_task_self(), cp);

	if (mr != MACH_MSG_SUCCESS) {
		return -1;
	}

	int n = rbuf.reply.count;
	if (n > max) {
		n = max;
	}
	for (int i = 0; i < n; i++) {
		out[i] = rbuf.reply.jobs[i];
	}
	return n;
}

/* Shared by start/stop/unload -- all three send a lctl_label_request and
 * get back a lctl_status_reply, differing only in msgh_id. */
static int
lctl_label_call(uint32_t msgh_id, const char *label)
{
	mach_port_t cp;
	if (lctl_get_control_port(&cp) != KERN_SUCCESS) {
		return -100;
	}

	struct lctl_label_request req;
	union {
		struct lctl_status_reply reply;
		char pad[sizeof(struct lctl_status_reply) + MAX_TRAILER_SIZE];
	} rbuf;
	mach_port_name_t reply_port = mach_reply_port();

	req.Head.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
	req.Head.msgh_size = (mach_msg_size_t)sizeof(req);
	req.Head.msgh_remote_port = cp;
	req.Head.msgh_local_port = (mach_port_t)reply_port;
	req.Head.msgh_voucher_port = MACH_PORT_NULL;
	req.Head.msgh_id = (mach_msg_id_t)msgh_id;
	req.NDR = kNDRRecord;
	strncpy(req.label, label, sizeof(req.label) - 1);
	req.label[sizeof(req.label) - 1] = '\0';

	mach_msg_return_t mr = mach_msg_overwrite(&req.Head, MACH_SEND_MSG | MACH_RCV_MSG,
	    (mach_msg_size_t)sizeof(req), (mach_msg_size_t)sizeof(rbuf),
	    reply_port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL,
	    &rbuf.reply.Head, (mach_msg_size_t)sizeof(rbuf));

	mach_port_deallocate(mach_task_self(), reply_port);
	mach_port_deallocate(mach_task_self(), cp);

	if (mr != MACH_MSG_SUCCESS) {
		return -100;
	}
	return rbuf.reply.status;
}

int
lctl_start(const char *label)
{
	return lctl_label_call(LCTL_START_MSGH_ID, label);
}

int
lctl_stop(const char *label)
{
	return lctl_label_call(LCTL_STOP_MSGH_ID, label);
}

int
lctl_unload(const char *label)
{
	return lctl_label_call(LCTL_UNLOAD_MSGH_ID, label);
}

int
lctl_load(const char *path, char *label_out, size_t label_out_sz)
{
	mach_port_t cp;
	if (lctl_get_control_port(&cp) != KERN_SUCCESS) {
		return -100;
	}

	struct lctl_load_request req;
	union {
		struct lctl_status_reply reply;
		char pad[sizeof(struct lctl_status_reply) + MAX_TRAILER_SIZE];
	} rbuf;
	mach_port_name_t reply_port = mach_reply_port();

	req.Head.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
	req.Head.msgh_size = (mach_msg_size_t)sizeof(req);
	req.Head.msgh_remote_port = cp;
	req.Head.msgh_local_port = (mach_port_t)reply_port;
	req.Head.msgh_voucher_port = MACH_PORT_NULL;
	req.Head.msgh_id = LCTL_LOAD_MSGH_ID;
	req.NDR = kNDRRecord;
	strncpy(req.path, path, sizeof(req.path) - 1);
	req.path[sizeof(req.path) - 1] = '\0';

	mach_msg_return_t mr = mach_msg_overwrite(&req.Head, MACH_SEND_MSG | MACH_RCV_MSG,
	    (mach_msg_size_t)sizeof(req), (mach_msg_size_t)sizeof(rbuf),
	    reply_port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL,
	    &rbuf.reply.Head, (mach_msg_size_t)sizeof(rbuf));

	mach_port_deallocate(mach_task_self(), reply_port);
	mach_port_deallocate(mach_task_self(), cp);

	if (mr != MACH_MSG_SUCCESS) {
		return -100;
	}
	if (rbuf.reply.status == 0 && label_out) {
		strncpy(label_out, rbuf.reply.label, label_out_sz - 1);
		label_out[label_out_sz - 1] = '\0';
	}
	return rbuf.reply.status;
}
