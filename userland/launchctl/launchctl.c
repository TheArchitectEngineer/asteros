/* Copyright (c) 2026 Vihaan Nathan
 *
 * launchctl: a real command-line client for launchd's control protocol
 * (userland/launchd/launchd_control.h) -- list/start/stop/load/unload,
 * the commonly-used core of real macOS launchctl's subcommand set. Talks
 * to launchd over genuine Mach IPC via the client stubs in
 * userland/launchd/launchd_control_client.c (shared with the
 * userland/launchd/test/launchctltest.c regression test), which in turn
 * reach launchd's control service by name through the real bootstrap-
 * namespace registry (mach/bootstrap.h, Phase 28) -- the exact same path
 * any other named service lookup takes, no special-casing.
 *
 * A static, raw-syscall binary (same build style as launchd/busybox --
 * see build.sh), not a dyld-linked one: real launchctl is a lightweight
 * standalone tool with no need for libxpc/Foundation, and launchd itself
 * (which this shares its client stub source with) is static for the same
 * reason.
 */
#include "launchd_control.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
usage(void)
{
	fprintf(stderr,
	    "usage: launchctl list [label]\n"
	    "       launchctl start <label>\n"
	    "       launchctl stop <label>\n"
	    "       launchctl load <plist>\n"
	    "       launchctl unload <label>\n");
}

static int
cmd_list(int argc, char **argv)
{
	struct lctl_job_wire jobs[LCTL_MAX_JOBS];
	int n = lctl_list(jobs, LCTL_MAX_JOBS);
	if (n < 0) {
		fprintf(stderr, "launchctl: list: could not reach launchd\n");
		return 1;
	}

	if (argc > 0) {
		const char *want = argv[0];
		for (int i = 0; i < n; i++) {
			if (strcmp(jobs[i].label, want) == 0) {
				printf("%s\n", jobs[i].label);
				if (jobs[i].pid > 0) {
					printf("\tpid = %d\n", jobs[i].pid);
				} else {
					printf("\tpid = (not running)\n");
				}
				printf("\tKeepAlive = %s\n", jobs[i].keep_alive ? "true" : "false");
				printf("\tRunAtLoad = %s\n", jobs[i].run_at_load ? "true" : "false");
				return 0;
			}
		}
		fprintf(stderr, "launchctl: list: could not find service \"%s\"\n", want);
		return 1;
	}

	printf("PID\tLabel\n");
	for (int i = 0; i < n; i++) {
		if (jobs[i].pid > 0) {
			printf("%d\t%s\n", jobs[i].pid, jobs[i].label);
		} else {
			printf("-\t%s\n", jobs[i].label);
		}
	}
	return 0;
}

static int
cmd_start(int argc, char **argv)
{
	if (argc < 1) {
		usage();
		return 1;
	}
	int status = lctl_start(argv[0]);
	if (status == 0) {
		return 0;
	}
	if (status == -1) {
		fprintf(stderr, "launchctl: start: could not find service \"%s\"\n", argv[0]);
	} else if (status == -2) {
		fprintf(stderr, "launchctl: start: \"%s\" is already running\n", argv[0]);
	} else {
		fprintf(stderr, "launchctl: start: could not reach launchd\n");
	}
	return 1;
}

static int
cmd_stop(int argc, char **argv)
{
	if (argc < 1) {
		usage();
		return 1;
	}
	int status = lctl_stop(argv[0]);
	if (status == 0) {
		return 0;
	}
	if (status == -1) {
		fprintf(stderr, "launchctl: stop: could not find service \"%s\"\n", argv[0]);
	} else if (status == -2) {
		fprintf(stderr, "launchctl: stop: \"%s\" is not running\n", argv[0]);
	} else {
		fprintf(stderr, "launchctl: stop: could not reach launchd\n");
	}
	return 1;
}

static int
cmd_load(int argc, char **argv)
{
	if (argc < 1) {
		usage();
		return 1;
	}
	char label[LCTL_LABEL_LEN];
	int status = lctl_load(argv[0], label, sizeof(label));
	if (status == 0) {
		printf("launchctl: loaded %s\n", label);
		return 0;
	}
	if (status == -1) {
		fprintf(stderr, "launchctl: load: could not parse \"%s\"\n", argv[0]);
	} else if (status == -2) {
		fprintf(stderr, "launchctl: load: a service with that Label is already loaded\n");
	} else if (status == -3) {
		fprintf(stderr, "launchctl: load: launchd's job table is full\n");
	} else {
		fprintf(stderr, "launchctl: load: could not reach launchd\n");
	}
	return 1;
}

static int
cmd_unload(int argc, char **argv)
{
	if (argc < 1) {
		usage();
		return 1;
	}
	int status = lctl_unload(argv[0]);
	if (status == 0) {
		return 0;
	}
	if (status == -1) {
		fprintf(stderr, "launchctl: unload: could not find service \"%s\"\n", argv[0]);
	} else {
		fprintf(stderr, "launchctl: unload: could not reach launchd\n");
	}
	return 1;
}

int
main(int argc, char **argv)
{
	if (argc < 2) {
		usage();
		return 1;
	}

	const char *cmd = argv[1];
	int rargc = argc - 2;
	char **rargv = argv + 2;

	if (strcmp(cmd, "list") == 0) {
		return cmd_list(rargc, rargv);
	} else if (strcmp(cmd, "start") == 0) {
		return cmd_start(rargc, rargv);
	} else if (strcmp(cmd, "stop") == 0) {
		return cmd_stop(rargc, rargv);
	} else if (strcmp(cmd, "load") == 0) {
		return cmd_load(rargc, rargv);
	} else if (strcmp(cmd, "unload") == 0) {
		return cmd_unload(rargc, rargv);
	}

	fprintf(stderr, "launchctl: unknown subcommand \"%s\"\n", cmd);
	usage();
	return 1;
}
