/* Copyright (c) 2026 Vihaan Nathan
 *
 * launchd's control server: the userland/launchctl/ half of
 * launchd_control.h's protocol. Same shape as bootstrap_server.c --
 * allocate a receive right, spin a dedicated pthread that loops
 * mach_msg(MACH_RCV_MSG) on it forever, demux by msgh_id -- except this
 * server's own receive-derived send right is itself published as a real
 * named service (LCTL_SERVICE_NAME) via bootstrap_register() against
 * launchd's own registry, exactly the way any other daemon publishes a
 * service. launchctl reaches it via the ordinary bootstrap_look_up()
 * path, no special-casing -- proof, incidentally, that the registry
 * genuinely works for a service launchd itself hosts, not just for
 * other daemons.
 *
 * Reply construction (msgh_remote_port = the received request's own
 * msgh_remote_port field, MOVE_SEND_ONCE) is the same machtest_main.c-
 * ground-truthed field swap bootstrap_server.c already uses.
 */
#include "control_server.h"
#include "launchd_control.h"
#include "launchd_ops.h"
#include <mach/bootstrap.h>
#include <mach/kern_return.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/mach_traps.h>
#include <mach/task_special_ports.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

static const NDR_record_t kNDRRecord = {0, 0, 0, 0, 0, 0, 0, 0};

union control_request {
	mach_msg_header_t Head;
	struct lctl_list_request list;
	struct lctl_label_request label;
	struct lctl_load_request load;
	char pad[sizeof(struct lctl_load_request) + MAX_TRAILER_SIZE];
};

static void
send_status_reply(mach_msg_header_t *req_head, uint32_t reply_id, int32_t status, const char *label)
{
	struct lctl_status_reply reply;
	reply.Head.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE, 0);
	reply.Head.msgh_size = (mach_msg_size_t)sizeof(reply);
	reply.Head.msgh_remote_port = req_head->msgh_remote_port;
	reply.Head.msgh_local_port = MACH_PORT_NULL;
	reply.Head.msgh_voucher_port = MACH_PORT_NULL;
	reply.Head.msgh_id = (mach_msg_id_t)reply_id;
	reply.NDR = kNDRRecord;
	reply.status = status;
	memset(reply.label, 0, sizeof(reply.label));
	if (label) {
		strncpy(reply.label, label, sizeof(reply.label) - 1);
	}

	kern_return_t kr = (kern_return_t)mach_msg(&reply.Head, MACH_SEND_MSG,
	    (mach_msg_size_t)sizeof(reply), 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "[launchd] control: status reply send failed kr=%d\n", kr);
	}
}

static void
handle_list(union control_request *req)
{
	struct lctl_list_reply reply;
	struct lc_job_info jobs[LCTL_MAX_JOBS];
	int n = lc_list(jobs, LCTL_MAX_JOBS);

	reply.Head.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE, 0);
	reply.Head.msgh_size = (mach_msg_size_t)sizeof(reply);
	reply.Head.msgh_remote_port = req->Head.msgh_remote_port;
	reply.Head.msgh_local_port = MACH_PORT_NULL;
	reply.Head.msgh_voucher_port = MACH_PORT_NULL;
	reply.Head.msgh_id = LCTL_LIST_MSGH_ID + 100;
	reply.NDR = kNDRRecord;
	reply.count = n;
	memset(reply.jobs, 0, sizeof(reply.jobs));
	for (int i = 0; i < n; i++) {
		strncpy(reply.jobs[i].label, jobs[i].label, sizeof(reply.jobs[i].label) - 1);
		reply.jobs[i].pid = (int32_t)jobs[i].pid;
		reply.jobs[i].keep_alive = jobs[i].keep_alive;
		reply.jobs[i].run_at_load = jobs[i].run_at_load;
	}

	kern_return_t kr = (kern_return_t)mach_msg(&reply.Head, MACH_SEND_MSG,
	    (mach_msg_size_t)sizeof(reply), 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "[launchd] control: list reply send failed kr=%d\n", kr);
	}
}

static void
handle_start(union control_request *req)
{
	char label[LCTL_LABEL_LEN];
	strncpy(label, req->label.label, sizeof(label) - 1);
	label[sizeof(label) - 1] = '\0';
	int status = lc_start(label);
	send_status_reply(&req->Head, LCTL_START_MSGH_ID + 100, status, NULL);
}

static void
handle_stop(union control_request *req)
{
	char label[LCTL_LABEL_LEN];
	strncpy(label, req->label.label, sizeof(label) - 1);
	label[sizeof(label) - 1] = '\0';
	int status = lc_stop(label);
	send_status_reply(&req->Head, LCTL_STOP_MSGH_ID + 100, status, NULL);
}

static void
handle_unload(union control_request *req)
{
	char label[LCTL_LABEL_LEN];
	strncpy(label, req->label.label, sizeof(label) - 1);
	label[sizeof(label) - 1] = '\0';
	int status = lc_unload(label);
	send_status_reply(&req->Head, LCTL_UNLOAD_MSGH_ID + 100, status, NULL);
}

static void
handle_load(union control_request *req)
{
	char path[LCTL_PATH_LEN];
	strncpy(path, req->load.path, sizeof(path) - 1);
	path[sizeof(path) - 1] = '\0';
	char label[LCTL_LABEL_LEN];
	label[0] = '\0';
	int status = lc_load(path, label, sizeof(label));
	send_status_reply(&req->Head, LCTL_LOAD_MSGH_ID + 100, status, label);
}

static mach_port_name_t g_control_recv_name;

static void *
control_thread_main(void *arg)
{
	(void)arg;
	for (;;) {
		union control_request req;
		kern_return_t kr = (kern_return_t)mach_msg(&req.Head, MACH_RCV_MSG, 0,
		    (mach_msg_size_t)sizeof(req), g_control_recv_name, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
		if (kr != KERN_SUCCESS) {
			fprintf(stderr, "[launchd] control: receive failed kr=%d\n", kr);
			continue;
		}

		switch (req.Head.msgh_id) {
		case LCTL_LIST_MSGH_ID:
			handle_list(&req);
			break;
		case LCTL_START_MSGH_ID:
			handle_start(&req);
			break;
		case LCTL_STOP_MSGH_ID:
			handle_stop(&req);
			break;
		case LCTL_LOAD_MSGH_ID:
			handle_load(&req);
			break;
		case LCTL_UNLOAD_MSGH_ID:
			handle_unload(&req);
			break;
		default:
			fprintf(stderr, "[launchd] control: unknown msgh_id 0x%x\n", (unsigned)req.Head.msgh_id);
			break;
		}
	}
	return NULL;
}

void
control_server_start(void)
{
	kern_return_t kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &g_control_recv_name);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "[launchd] control: mach_port_allocate failed kr=%d\n", kr);
		return;
	}
	kr = mach_port_insert_right(mach_task_self(), g_control_recv_name, g_control_recv_name, MACH_MSG_TYPE_MAKE_SEND);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "[launchd] control: mach_port_insert_right failed kr=%d\n", kr);
		return;
	}

	mach_port_t bp = MACH_PORT_NULL;
	kr = task_get_special_port(mach_task_self(), TASK_BOOTSTRAP_PORT, &bp);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "[launchd] control: task_get_special_port failed kr=%d\n", kr);
		return;
	}
	kr = bootstrap_register(bp, LCTL_SERVICE_NAME, (mach_port_t)g_control_recv_name);
	mach_port_deallocate(mach_task_self(), bp);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "[launchd] control: bootstrap_register failed kr=%d\n", kr);
		return;
	}

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_t t;
	pthread_create(&t, &attr, control_thread_main, NULL);
	pthread_attr_destroy(&attr);
}
