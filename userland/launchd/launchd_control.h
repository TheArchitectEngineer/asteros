/* Copyright (c) 2026 Vihaan Nathan
 *
 * launchd's control protocol: what launchctl (userland/launchctl/) speaks
 * to launchd's control_server.c over. Same tier as mach/bootstrap.h's own
 * protocol -- hand-marshaled, own msgh_id numbering, own struct shapes,
 * "nothing outside this OS's own process pairs ever needs to decode it".
 * launchd itself is a fully static, no-dyld binary (see the Makefile's
 * launchd rule), so this can't be libxpc -- it has to live at the same
 * raw-Mach-message level bootstrap.h already established.
 *
 * The service itself is just another named bootstrap-namespace entry
 * (LCTL_SERVICE_NAME, registered by control_server.c via
 * bootstrap_register() against launchd's own registry) -- launchctl
 * reaches it exactly the way any other client reaches any other named
 * service, via bootstrap_look_up(). No special-casing.
 *
 * Every request here is SIMPLE (no port descriptors -- this protocol only
 * ever moves fixed-size data, never rights) and every reply is padded
 * with MAX_TRAILER_SIZE headroom for the kernel-appended receive trailer,
 * same as every other receive in this tree (see machtest_main.c's file
 * header for why that padding exists at all).
 */
#ifndef LAUNCHD_CONTROL_H
#define LAUNCHD_CONTROL_H

#include <mach/message.h>
#include <mach/ndr.h>
#include <mach/port.h>
#include <stddef.h>
#include <stdint.h>

#define LCTL_SERVICE_NAME "com.asteros.launchd.control"

#define LCTL_LIST_MSGH_ID   9200
#define LCTL_START_MSGH_ID  9201
#define LCTL_STOP_MSGH_ID   9202
#define LCTL_LOAD_MSGH_ID   9203
#define LCTL_UNLOAD_MSGH_ID 9204

#define LCTL_MAX_JOBS  64
#define LCTL_LABEL_LEN 128
#define LCTL_PATH_LEN  256

struct lctl_job_wire {
	char label[LCTL_LABEL_LEN];
	int32_t pid;         /* 0 if not running */
	int32_t keep_alive;
	int32_t run_at_load;
};

struct lctl_list_request {
	mach_msg_header_t Head;
	NDR_record_t NDR;
};

struct lctl_list_reply {
	mach_msg_header_t Head;
	NDR_record_t NDR;
	int32_t count;
	struct lctl_job_wire jobs[LCTL_MAX_JOBS];
};

/* START/STOP/UNLOAD: request carries just a label. */
struct lctl_label_request {
	mach_msg_header_t Head;
	NDR_record_t NDR;
	char label[LCTL_LABEL_LEN];
};

/* LOAD: request carries a plist path instead. */
struct lctl_load_request {
	mach_msg_header_t Head;
	NDR_record_t NDR;
	char path[LCTL_PATH_LEN];
};

/* Shared reply shape for START/STOP/UNLOAD/LOAD -- `status` is the
 * corresponding lc_*() return value (0 == success); `label` is only
 * meaningful in LOAD's reply (the Label the plist parsed to). */
struct lctl_status_reply {
	mach_msg_header_t Head;
	NDR_record_t NDR;
	int32_t status;
	char label[LCTL_LABEL_LEN];
};

/* ---- client stubs (userland/launchd/launchd_control_client.c) ----
 * Each resolves LCTL_SERVICE_NAME fresh via bootstrap_look_up() on every
 * call -- same "no caching, full round trip every time" precedent
 * mach_special_ports.c's task_get_special_port already established.
 * Shared between launchctl.c and launchctltest.c (userland/launchd/test).
 */

/* Returns the number of jobs written to `out` (capacity `max`, itself
 * capped at LCTL_MAX_JOBS), or -1 on any IPC-level failure (service not
 * reachable, etc). */
int lctl_list(struct lctl_job_wire *out, int max);

/* Returns the corresponding lc_*() status code (0 == success, negative
 * == the specific failure -- see launchd_ops.h), or -100 if the request
 * itself couldn't be sent/answered at all (distinct range so IPC failure
 * is never confused with a real lc_*() result). */
int lctl_start(const char *label);
int lctl_stop(const char *label);
int lctl_unload(const char *label);

/* On success (return 0), copies the parsed Label into `label_out`
 * (capacity `label_out_sz`). */
int lctl_load(const char *path, char *label_out, size_t label_out_sz);

#endif /* LAUNCHD_CONTROL_H */
