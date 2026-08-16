/* Copyright (c) 2026 Vihaan Nathan
 *
 * End-to-end proof that launchd's control protocol (launchd_control.h,
 * served by control_server.c, reached via the real bootstrap-namespace
 * registry -- Phase 28 -- exactly like any other named service) is real:
 * writes a fresh LaunchDaemon plist to /tmp at runtime, then drives it
 * through the full load -> list -> start -> list -> stop -> list ->
 * unload -> list lifecycle using the exact same client stubs
 * (userland/launchd/launchd_control_client.c) launchctl itself uses --
 * this is not a loopback shortcut, every call is a genuine Mach IPC round
 * trip to the live launchd process. A final lctl_start() against a label
 * that was never loaded proves the "not found" error path is real too.
 */
#include "launchd_control.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define TEST_LABEL "com.asteros.launchctltest.dynamic"
#define TEST_PLIST_PATH "/tmp/launchctltest.plist"

static int
write_test_plist(void)
{
	FILE *f = fopen(TEST_PLIST_PATH, "w");
	if (!f) {
		return -1;
	}
	/* "cat" with no arguments blocks reading from stdin (inherited from
	 * launchd -- /dev/console, never hits EOF) -- a long-running child
	 * this project's busybox build actually has compiled in (see
	 * TODO.md Phase 9's enabled-applet list: no "sleep"), perfect for
	 * observing a real start/stop lifecycle. */
	fprintf(f,
	    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	    "<plist version=\"1.0\">\n"
	    "<dict>\n"
	    "\t<key>Label</key>\n"
	    "\t<string>%s</string>\n"
	    "\t<key>ProgramArguments</key>\n"
	    "\t<array>\n"
	    "\t\t<string>/bin/busybox</string>\n"
	    "\t\t<string>cat</string>\n"
	    "\t</array>\n"
	    "\t<key>RunAtLoad</key>\n"
	    "\t<false/>\n"
	    "\t<key>KeepAlive</key>\n"
	    "\t<false/>\n"
	    "</dict>\n"
	    "</plist>\n",
	    TEST_LABEL);
	fclose(f);
	return 0;
}

/* Returns 1 if TEST_LABEL is present, and if so writes its pid to *pid_out
 * (0 if not running). Returns 0 if absent, -1 on IPC failure. */
static int
find_test_job(int32_t *pid_out)
{
	struct lctl_job_wire jobs[LCTL_MAX_JOBS];
	int n = lctl_list(jobs, LCTL_MAX_JOBS);
	if (n < 0) {
		return -1;
	}
	for (int i = 0; i < n; i++) {
		if (strcmp(jobs[i].label, TEST_LABEL) == 0) {
			if (pid_out) {
				*pid_out = jobs[i].pid;
			}
			return 1;
		}
	}
	return 0;
}

int
main(void)
{
	/* Sanity: shouldn't already be loaded (this is a fresh label, no
	 * boot-time plist declares it). */
	int present = find_test_job(NULL);
	if (present != 0) {
		printf("LAUNCHCTLTEST FAIL: \"%s\" unexpectedly already loaded (present=%d)\n", TEST_LABEL, present);
		return 1;
	}

	if (write_test_plist() != 0) {
		printf("LAUNCHCTLTEST FAIL: could not write %s\n", TEST_PLIST_PATH);
		return 1;
	}

	char label_out[LCTL_LABEL_LEN];
	int status = lctl_load(TEST_PLIST_PATH, label_out, sizeof(label_out));
	if (status != 0 || strcmp(label_out, TEST_LABEL) != 0) {
		printf("LAUNCHCTLTEST FAIL: lctl_load status=%d label_out=%s\n", status, label_out);
		return 1;
	}

	int32_t pid = -1;
	present = find_test_job(&pid);
	if (present != 1 || pid != 0) {
		printf("LAUNCHCTLTEST FAIL: after load, present=%d pid=%d (want present=1 pid=0)\n", present, pid);
		return 1;
	}

	status = lctl_start(TEST_LABEL);
	if (status != 0) {
		printf("LAUNCHCTLTEST FAIL: lctl_start status=%d\n", status);
		return 1;
	}

	pid = 0;
	present = find_test_job(&pid);
	if (present != 1 || pid <= 0) {
		printf("LAUNCHCTLTEST FAIL: after start, present=%d pid=%d (want present=1 pid>0)\n", present, pid);
		return 1;
	}

	/* Starting an already-running job is a documented no-op error
	 * (status -2), not a second fork -- confirms lc_start()'s
	 * already-running guard is real. */
	status = lctl_start(TEST_LABEL);
	if (status != -2) {
		printf("LAUNCHCTLTEST FAIL: lctl_start on an already-running job returned %d (want -2)\n", status);
		return 1;
	}

	status = lctl_stop(TEST_LABEL);
	if (status != 0) {
		printf("LAUNCHCTLTEST FAIL: lctl_stop status=%d\n", status);
		return 1;
	}

	/* Bounded poll for launchd's reap loop to actually collect the exit
	 * -- same idiom userland/libxpc/test/xpctest.c already uses for its
	 * own async-delivery check. */
	int reaped = 0;
	for (int i = 0; i < 100; i++) {
		pid = -1;
		present = find_test_job(&pid);
		if (present == 1 && pid == 0) {
			reaped = 1;
			break;
		}
		usleep(20000);
	}
	if (!reaped) {
		printf("LAUNCHCTLTEST FAIL: job never reaped after stop (last pid=%d)\n", pid);
		return 1;
	}

	status = lctl_unload(TEST_LABEL);
	if (status != 0) {
		printf("LAUNCHCTLTEST FAIL: lctl_unload status=%d\n", status);
		return 1;
	}

	present = find_test_job(NULL);
	if (present != 0) {
		printf("LAUNCHCTLTEST FAIL: still present after unload (present=%d)\n", present);
		return 1;
	}

	/* A label that was never loaded at all: the "not found" error path,
	 * not a silent success or a crash. */
	status = lctl_start("com.asteros.launchctltest.nonexistent");
	if (status != -1) {
		printf("LAUNCHCTLTEST FAIL: lctl_start on a never-loaded label returned %d (want -1)\n", status);
		return 1;
	}

	printf("LAUNCHCTLTEST PASS\n");
	return 0;
}
