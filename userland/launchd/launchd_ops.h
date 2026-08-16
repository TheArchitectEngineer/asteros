/* Copyright (c) 2026 Vihaan Nathan
 *
 * launchd's internal daemon-table operations, exposed for
 * control_server.c to drive on launchctl's behalf. Each lc_* function
 * takes launchd.c's own g_daemons_lock internally -- callers don't need
 * to know the table is mutex-guarded, just that these calls may block
 * briefly (bounded: at most spawn_daemon()'s respawn-throttle sleep).
 */
#ifndef LAUNCHD_OPS_H
#define LAUNCHD_OPS_H

#include <stddef.h>
#include <sys/types.h>

#define LC_MAX_LIST 64

struct lc_job_info {
	char label[128];
	pid_t pid;       /* 0 if not currently running */
	int keep_alive;
	int run_at_load;
};

/* Fills `out` (capacity `max`) with every loaded (non-unloaded) daemon,
 * returns the count written (never more than `max` or LC_MAX_LIST). */
int lc_list(struct lc_job_info *out, int max);

/* 0: started. -1: no such label. -2: already running (no-op, matches
 * real launchctl's "start" on an already-running job). */
int lc_start(const char *label);

/* 0: signaled. -1: no such label. -2: not currently running. */
int lc_stop(const char *label);

/* Parses the plist at `path` and adds it to the table (spawning it
 * immediately if it declares RunAtLoad, same as boot-time loading).
 * On success, copies the parsed Label into `label_out` (capacity
 * `label_out_sz`). 0: loaded. -1: parse failure. -2: label already
 * loaded. -3: table full (MAX_DAEMONS). */
int lc_load(const char *path, char *label_out, size_t label_out_sz);

/* Stops (if running) and removes `label` from supervision -- it will not
 * be respawned even if KeepAlive, and its label becomes loadable again.
 * 0: unloaded. -1: no such label. */
int lc_unload(const char *label);

#endif /* LAUNCHD_OPS_H */
