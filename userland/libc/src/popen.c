/* Real fork/pipe/exec popen()/pclose() -- added for the X11 milestone
 * (xterm's print.c pipes formatted output to a printer command). Tracks
 * child pids in a small static table since pclose() needs the pid but
 * only gets handed the FILE*, same approach glibc/BSD libc use. */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#define MAX_POPEN 16

static struct {
	FILE *fp;
	pid_t pid;
} popen_table[MAX_POPEN];

FILE *
popen(const char *command, const char *type)
{
	int fds[2];
	int reading = (type[0] == 'r');

	if (pipe(fds) < 0) {
		return NULL;
	}

	pid_t pid = fork();
	if (pid < 0) {
		close(fds[0]);
		close(fds[1]);
		return NULL;
	}

	if (pid == 0) {
		if (reading) {
			dup2(fds[1], STDOUT_FILENO);
		} else {
			dup2(fds[0], STDIN_FILENO);
		}
		close(fds[0]);
		close(fds[1]);
		execl("/bin/sh", "sh", "-c", command, (char *)NULL);
		_exit(127);
	}

	int keep = reading ? fds[0] : fds[1];
	int drop = reading ? fds[1] : fds[0];
	close(drop);

	FILE *fp = fdopen(keep, reading ? "r" : "w");
	if (!fp) {
		close(keep);
		return NULL;
	}

	for (int i = 0; i < MAX_POPEN; i++) {
		if (!popen_table[i].fp) {
			popen_table[i].fp = fp;
			popen_table[i].pid = pid;
			break;
		}
	}

	return fp;
}

int
pclose(FILE *stream)
{
	pid_t pid = -1;

	for (int i = 0; i < MAX_POPEN; i++) {
		if (popen_table[i].fp == stream) {
			pid = popen_table[i].pid;
			popen_table[i].fp = NULL;
			break;
		}
	}

	fclose(stream);

	if (pid < 0) {
		return -1;
	}

	int status;
	if (waitpid(pid, &status, 0) < 0) {
		return -1;
	}
	return status;
}
