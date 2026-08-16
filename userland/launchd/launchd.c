/* Copyright (c) 2026 Vihaan Nathan
 *
 * launchd: PID 1. Replaces userland/init_launcher.c (retired -- this
 * absorbs its bootstrap directly). Loads every /etc/launchd/daemons/
 * *.plist, forks+execs the RunAtLoad ones, reaps children in a blocking
 * loop, and re-forks any KeepAlive daemon that exits. SIGTERM begins a
 * bounded shutdown: signal every child, drain exits, reboot(RB_HALT).
 *
 * Real xnu's load_init_program() (src/xnu/bsd/kern/kern_exec.c) already
 * tries /sbin/launchd before falling back to /sbin/init -- ground-truthed
 * by reading it, not assumed -- so this binary ships at /sbin/launchd and
 * needs no kernel changes to be picked up as PID 1.
 */
#define __APPLE_API_PRIVATE /* for RB_HALT + the reboot() prototype */
#include <sys/reboot.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/sysctl.h>
#include <pthread.h>
#include "plist.h"
#include "bootstrap_server.h"
#include "control_server.h"
#include "launchd_ops.h"

#define MAX_DAEMONS      64
#define DAEMONS_DIR      "/etc/launchd/daemons"
#define LOG_PATH         "/var/log/launchd.log"
#define RESPAWN_THROTTLE_MS 500 /* see spawn_daemon()'s comment */

struct daemon {
	struct daemon_config cfg;
	pid_t pid; /* 0 if not currently running */
	long long last_spawn_ms;
	int unloaded; /* lc_unload()'d -- excluded from listing/respawn/shutdown,
	               * and its label becomes loadable again via lc_load(). Not
	               * physically removed from g_daemons[]: this is a fixed
	               * array other code holds pointers into (find_by_pid()'s
	               * return value in particular), so a flag avoids every
	               * index-stability hazard a real compaction would add for
	               * no real benefit at this scale. */
};

/* Guards every access to g_daemons[]/g_ndaemons -- both the main
 * supervision loop (below) and control_server.c's dedicated thread (which
 * drives the lc_* functions on launchctl's behalf) touch this table now.
 * Contract: find_by_pid()/find_by_label()/running_count()/spawn_daemon()
 * assume the lock is already held by their caller (small helpers, not
 * worth making individually self-locking); the lc_* functions and every
 * other direct table access take it themselves. */
static pthread_mutex_t g_daemons_lock = PTHREAD_MUTEX_INITIALIZER;

static long long
monotonic_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* Blocks until monotonic_ms() reaches target_ms. usleep() does the real
 * waiting (confirmed accurate live in QEMU by reading timestamps back
 * out of /var/log/launchd.log, not just guessed at); the trailing spin
 * is a cheap correctness backstop against usleep()'s setitimer()+
 * sigsuspend() implementation waking early for any reason (e.g. an
 * unrelated signal interrupting the sigsuspend -- see time.c's
 * nanosleep() for the related bug that mattered more), not evidence
 * that it actually does. */
static void
sleep_until_ms(long long target_ms)
{
	long long now = monotonic_ms();
	if (target_ms > now) {
		usleep((unsigned)(target_ms - now) * 1000);
	}
	while (monotonic_ms() < target_ms) {
	}
}

static struct daemon g_daemons[MAX_DAEMONS];
static int g_ndaemons;
static int g_logfd = -1;
static volatile int g_shutdown_requested;
static volatile int g_alarm_fired;

/* Every event goes to LOG_PATH -- that's the "structured logging" this
 * daemon actually promises. Only to_console==1 callers also echo to fd 1
 * (the shared physical console the interactive shell daemon lives on
 * too): ground-truthed in QEMU that a KeepAlive daemon respawning even
 * once a second forever makes the console permanently unusable if every
 * routine child start/exit is echoed there. launchd's own top-level
 * lifecycle events (boot, shutdown) still go to console; the noisy,
 * frequent per-daemon ones (spawn/exit) go to the file only -- still
 * fully visible via `cat /var/log/launchd.log`. */
static void
vllog(const char *label, int to_console, const char *fmt, va_list ap)
{
	char msg[400];
	vsnprintf(msg, sizeof(msg), fmt, ap);

	char line[512];
	int n = snprintf(line, sizeof(line), "[launchd] %ld %s: %s\n",
	    (long)time(NULL), label ? label : "-", msg);
	if (n > 0) {
		if (to_console) {
			write(1, line, (size_t)n);
		}
		if (g_logfd >= 0) {
			write(g_logfd, line, (size_t)n);
		}
	}
}

static void
llog(const char *label, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vllog(label, 0, fmt, ap);
	va_end(ap);
}

static void
llog_console(const char *label, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vllog(label, 1, fmt, ap);
	va_end(ap);
}

static void
on_sigterm(int sig)
{
	(void)sig;
	g_shutdown_requested = 1;
}

static void
on_sigalrm(int sig)
{
	(void)sig;
	g_alarm_fired = 1;
}

/* mount devfs and claim fd 0/1/2 on /dev/console -- PID 1 is exec'd
 * directly by the kernel with no inherited fd table, same reasoning
 * userland/init_launcher.c documented before this absorbed it. */
static void
bootstrap_console(void)
{
	mount("devfs", "/dev", 0, 0);
	open("/dev/console", O_RDWR, 0);
	open("/dev/console", O_RDWR, 0);
	open("/dev/console", O_RDWR, 0);
	g_logfd = open(LOG_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (g_logfd < 0) {
		/* llog() isn't safe to call yet (it unconditionally tries to
		 * write to g_logfd) -- and silently losing every future log
		 * line to a missing /var/log is exactly the kind of failure
		 * that should be loud, not swallowed (ground-truthed: an
		 * earlier rootfs build never created /var/log at all, and nothing
		 * surfaced that until log files turned up empty in a live boot). */
		static const char msg[] = "[launchd] warning: could not open " LOG_PATH ", logging to console only\n";
		write(1, msg, sizeof(msg) - 1);
	}
}

static int
cmp_str(const void *a, const void *b)
{
	return strcmp(*(char *const *)a, *(char *const *)b);
}

/* Loads every *.plist in DAEMONS_DIR, sorted by filename -- this
 * project's v1 stand-in for real dependency ordering (no Requires-style
 * key support yet, so filename order is the only ordering knob). */
static void
load_all_daemons(void)
{
	DIR *d = opendir(DAEMONS_DIR);
	if (!d) {
		llog_console("launchd", "cannot open %s, no daemons loaded", DAEMONS_DIR);
		return;
	}

	char *names[MAX_DAEMONS];
	int nnames = 0;
	struct dirent *de;
	while ((de = readdir(d)) != NULL) {
		size_t len = strlen(de->d_name);
		if (len < 7 || strcmp(de->d_name + len - 6, ".plist") != 0) {
			continue;
		}
		if (nnames < MAX_DAEMONS) {
			names[nnames++] = strdup(de->d_name);
		}
	}
	closedir(d);

	qsort(names, (size_t)nnames, sizeof(names[0]), cmp_str);

	pthread_mutex_lock(&g_daemons_lock);
	for (int i = 0; i < nnames; i++) {
		if (g_ndaemons >= MAX_DAEMONS) {
			llog_console("launchd", "MAX_DAEMONS reached, ignoring %s", names[i]);
			free(names[i]);
			continue;
		}
		char path[256];
		snprintf(path, sizeof(path), "%s/%s", DAEMONS_DIR, names[i]);
		struct daemon *dm = &g_daemons[g_ndaemons];
		if (plist_parse_daemon(path, &dm->cfg) == 0) {
			dm->pid = 0;
			dm->unloaded = 0;
			g_ndaemons++;
			llog_console("launchd", "loaded %s (label=%s)", names[i], dm->cfg.label);
		} else {
			llog_console("launchd", "failed to parse %s, skipping", names[i]);
		}
		free(names[i]);
	}
	pthread_mutex_unlock(&g_daemons_lock);
}

/* Assumes g_daemons_lock is already held -- see its declaration comment. */
static struct daemon *
find_by_pid(pid_t pid)
{
	for (int i = 0; i < g_ndaemons; i++) {
		if (!g_daemons[i].unloaded && g_daemons[i].pid == pid) {
			return &g_daemons[i];
		}
	}
	return NULL;
}

/* Assumes g_daemons_lock is already held. */
static struct daemon *
find_by_label(const char *label)
{
	for (int i = 0; i < g_ndaemons; i++) {
		if (!g_daemons[i].unloaded && strcmp(g_daemons[i].cfg.label, label) == 0) {
			return &g_daemons[i];
		}
	}
	return NULL;
}

/* Assumes g_daemons_lock is already held. */
static int
running_count(void)
{
	int n = 0;
	for (int i = 0; i < g_ndaemons; i++) {
		if (!g_daemons[i].unloaded && g_daemons[i].pid > 0) {
			n++;
		}
	}
	return n;
}

/* Forks+execs d. Assumes g_daemons_lock is already held by the caller --
 * deliberately not self-locking (the respawn-throttle sleep below can
 * take up to RESPAWN_THROTTLE_MS, and every caller needs `d`'s fields
 * left undisturbed by a concurrent lc_* call for that whole span, not
 * just around the individual field writes).
 *
 * If the previous run of this same daemon exited less
 * than RESPAWN_THROTTLE_MS ago, sleeps out the remainder first --
 * ground-truthed as necessary, not theoretical: an early version of
 * echotest.c (see test/) exits in well under a millisecond, and without
 * this a KeepAlive daemon that dies instantly re-forks thousands of
 * times a minute, flooding the console/log and burning CPU. Real
 * launchd's actual crash-loop backoff is exponential across repeated
 * failures; this is a fixed single-interval simplification, documented
 * as a v1 gap in TODO.md Phase 14. */
static void
spawn_daemon(struct daemon *d)
{
	if (d->last_spawn_ms != 0) {
		sleep_until_ms(d->last_spawn_ms + RESPAWN_THROTTLE_MS);
	}

	pid_t pid = fork();
	if (pid < 0) {
		llog(d->cfg.label, "fork failed");
		return;
	}
	if (pid == 0) {
		if (d->cfg.stdout_path[0]) {
			int fd = open(d->cfg.stdout_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
			if (fd >= 0) {
				dup2(fd, 1);
				if (fd != 1) {
					close(fd);
				}
			}
		}
		if (d->cfg.stderr_path[0]) {
			int fd = open(d->cfg.stderr_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
			if (fd >= 0) {
				dup2(fd, 2);
				if (fd != 2) {
					close(fd);
				}
			}
		}
		execve(d->cfg.argv[0], d->cfg.argv, d->cfg.envp[0] ? d->cfg.envp : environ);
		_exit(127); /* execve only returns on failure */
	}
	d->pid = pid;
	d->last_spawn_ms = monotonic_ms();
	llog(d->cfg.label, "started pid %d", pid);
}

/* Quiet boot (see boot/boot.c) hides the console behind a splash by
 * telling xnu to use KBOOT_GRAPHICS_MODE, which -- unlike real macOS,
 * which has a WindowServer/loginwindow to paint over the boot picture
 * once it's ready -- this kernel never undoes on its own. kern.consoletext
 * (bsd/kern/kern_sysctl.c) is the escape hatch: writing any nonzero value
 * calls initialize_screen(NULL, kPETextScreen) and reveals the text
 * console. Called right before starting com.asteros.shell specifically
 * (not e.g. after load_all_daemons()) so the splash stays up for exactly
 * as long as the boot-time test daemons (cftest/pthreadtest/
 * foundationtest/echotest) are still doing their thing, and disappears
 * right as the interactive shell is about to become visible. A failed
 * write (e.g. verbose boot, where the console was never hidden to begin
 * with) is harmless -- ignored. */
static void
reveal_console(void)
{
	int one = 1;
	sysctlbyname("kern.consoletext", NULL, NULL, &one, sizeof(one));
}

static void
start_runatload_daemons(void)
{
	pthread_mutex_lock(&g_daemons_lock);
	for (int i = 0; i < g_ndaemons; i++) {
		if (g_daemons[i].cfg.run_at_load) {
			if (strcmp(g_daemons[i].cfg.label, "com.asteros.shell") == 0) {
				reveal_console();
			}
			spawn_daemon(&g_daemons[i]);
		}
	}
	pthread_mutex_unlock(&g_daemons_lock);
}

/* ---- launchd_ops.h: driven by control_server.c on launchctl's behalf ---- */

int
lc_list(struct lc_job_info *out, int max)
{
	if (max > LC_MAX_LIST) {
		max = LC_MAX_LIST;
	}
	pthread_mutex_lock(&g_daemons_lock);
	int n = 0;
	for (int i = 0; i < g_ndaemons && n < max; i++) {
		if (g_daemons[i].unloaded) {
			continue;
		}
		strncpy(out[n].label, g_daemons[i].cfg.label, sizeof(out[n].label) - 1);
		out[n].label[sizeof(out[n].label) - 1] = '\0';
		out[n].pid = g_daemons[i].pid;
		out[n].keep_alive = g_daemons[i].cfg.keep_alive;
		out[n].run_at_load = g_daemons[i].cfg.run_at_load;
		n++;
	}
	pthread_mutex_unlock(&g_daemons_lock);
	return n;
}

int
lc_start(const char *label)
{
	pthread_mutex_lock(&g_daemons_lock);
	struct daemon *d = find_by_label(label);
	if (!d) {
		pthread_mutex_unlock(&g_daemons_lock);
		return -1;
	}
	if (d->pid > 0) {
		pthread_mutex_unlock(&g_daemons_lock);
		return -2;
	}
	spawn_daemon(d);
	pthread_mutex_unlock(&g_daemons_lock);
	return 0;
}

int
lc_stop(const char *label)
{
	pthread_mutex_lock(&g_daemons_lock);
	struct daemon *d = find_by_label(label);
	if (!d) {
		pthread_mutex_unlock(&g_daemons_lock);
		return -1;
	}
	if (d->pid <= 0) {
		pthread_mutex_unlock(&g_daemons_lock);
		return -2;
	}
	kill(d->pid, SIGTERM);
	pthread_mutex_unlock(&g_daemons_lock);
	return 0;
}

int
lc_load(const char *path, char *label_out, size_t label_out_sz)
{
	struct daemon_config cfg;
	if (plist_parse_daemon(path, &cfg) != 0) {
		return -1;
	}

	pthread_mutex_lock(&g_daemons_lock);
	if (find_by_label(cfg.label)) {
		pthread_mutex_unlock(&g_daemons_lock);
		return -2;
	}
	if (g_ndaemons >= MAX_DAEMONS) {
		pthread_mutex_unlock(&g_daemons_lock);
		return -3;
	}
	struct daemon *d = &g_daemons[g_ndaemons++];
	d->cfg = cfg;
	d->pid = 0;
	d->last_spawn_ms = 0;
	d->unloaded = 0;
	llog("launchd", "loaded %s (label=%s) via launchctl", path, cfg.label);
	if (cfg.run_at_load) {
		spawn_daemon(d);
	}
	pthread_mutex_unlock(&g_daemons_lock);

	if (label_out) {
		strncpy(label_out, cfg.label, label_out_sz - 1);
		label_out[label_out_sz - 1] = '\0';
	}
	return 0;
}

int
lc_unload(const char *label)
{
	pthread_mutex_lock(&g_daemons_lock);
	struct daemon *d = find_by_label(label);
	if (!d) {
		pthread_mutex_unlock(&g_daemons_lock);
		return -1;
	}
	/* Set unloaded before signaling: by the time the child actually
	 * exits and the main reap loop reclaims its pid, it already sees
	 * unloaded==1 (both under this same lock) and won't respawn it even
	 * if KeepAlive is set -- no exit/respawn race. */
	d->unloaded = 1;
	if (d->pid > 0) {
		kill(d->pid, SIGTERM);
	}
	llog("launchd", "unloaded %s via launchctl", label);
	pthread_mutex_unlock(&g_daemons_lock);
	return 0;
}

/* SIGTERM handler: signal every supervised child, then drain exits with
 * a real bound -- alarm(2)/SIGALRM interrupts a blocking wait4() that
 * would otherwise hang forever on a child that ignores SIGTERM. No
 * SIGKILL escalation after the alarm fires (documented v1 gap): whatever
 * hasn't exited by then is left for the kernel to tear down at reboot. */
static void
do_shutdown(void)
{
	llog_console("launchd", "shutdown requested, signaling %d running daemon(s)", running_count());
	for (int i = 0; i < g_ndaemons; i++) {
		if (g_daemons[i].pid > 0) {
			kill(g_daemons[i].pid, SIGTERM);
		}
	}

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_sigalrm;
	sigaction(SIGALRM, &sa, NULL);
	g_alarm_fired = 0;
	alarm(5);

	while (!g_alarm_fired && running_count() > 0) {
		int status;
		pid_t pid = wait4(-1, &status, 0, NULL);
		if (pid <= 0) {
			break; /* ECHILD or EINTR (the alarm) -- nothing more to drain */
		}
		struct daemon *d = find_by_pid(pid);
		if (d) {
			d->pid = 0;
			llog(d->cfg.label, "exited during shutdown (status 0x%x)", status);
		}
	}
	alarm(0);

	llog_console("launchd", "halting");
	reboot(RB_HALT);
	/* reboot() failed for some reason -- spin rather than let PID 1
	 * exit, same fallback userland/init_launcher.c used. */
	for (;;) {
	}
}

int
main(void)
{
	bootstrap_console();
	llog_console("launchd", "starting");

	/* Must happen before the first spawn_daemon() (inside
	 * start_runatload_daemons() below) -- every daemon forked afterward
	 * inherits launchd's TASK_BOOTSTRAP_PORT via the kernel's
	 * ipc_task_init() parent-copy, so this has to be in place first. */
	bootstrap_server_start();
	/* Can start any time after bootstrap_server_start() -- launchctl
	 * invocations only ever happen post-boot, not via fork inheritance,
	 * so unlike bootstrap_server_start() this has no ordering
	 * requirement relative to load_all_daemons()/start_runatload_daemons(). */
	control_server_start();

	load_all_daemons();
	start_runatload_daemons();

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_sigterm;
	sigaction(SIGTERM, &sa, NULL);

	for (;;) {
		if (g_shutdown_requested) {
			do_shutdown();
			break;
		}

		int status;
		pid_t pid = wait4(-1, &status, 0, NULL);
		if (pid < 0) {
			continue; /* EINTR (a signal, re-check g_shutdown_requested) or
			           * ECHILD (no children right now) -- either way, loop */
		}

		/* PID 1 must reap every exited process, including ones that
		 * aren't a tracked daemon (e.g. an orphan reparented to us) --
		 * only re-spawn the ones we're actually supervising. */
		struct daemon *d = find_by_pid(pid);
		if (!d) {
			continue;
		}

		llog(d->cfg.label, "exited (status 0x%x)", status);
		d->pid = 0;
		if (d->cfg.keep_alive) {
			spawn_daemon(d);
		}
	}

	return 0;
}
