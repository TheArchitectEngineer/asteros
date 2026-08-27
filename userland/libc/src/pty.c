/* Real BSD-style openpty()/login_tty()/forkpty(), targeting this kernel's
 * legacy /dev/pty<letter><hex> + /dev/tty<letter><hex> device pairs (see
 * src/xnu/bsd/kern/tty_pty.c's pty_init()/pty_get_name() -- START_CHAR='p',
 * two hex digits' worth of minors per letter, world-readable/writable
 * 0666 nodes, no grantpt/unlockpt dance needed since each pair is a
 * distinct static device rather than a /dev/ptmx clone). Added for the
 * X11 milestone (xterm needs a real pty to run the shell it emulates). */
#include <util.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

int
openpty(int *amaster, int *aslave, char *name, struct termios *termp, struct winsize *winp)
{
	char mpath[32], spath[32];
	int master, slave;

	for (char c1 = 'p'; c1 <= 'z'; c1++) {
		for (int c2 = 0; c2 < 16; c2++) {
			snprintf(mpath, sizeof(mpath), "/dev/pty%c%x", c1, c2);
			master = open(mpath, O_RDWR, 0);
			if (master < 0) {
				continue;
			}

			snprintf(spath, sizeof(spath), "/dev/tty%c%x", c1, c2);
			slave = open(spath, O_RDWR, 0);
			if (slave < 0) {
				close(master);
				continue;
			}

			if (termp) {
				tcsetattr(slave, TCSANOW, termp);
			}
			if (winp) {
				ioctl(slave, TIOCSWINSZ, winp);
			}

			*amaster = master;
			*aslave = slave;
			if (name) {
				strcpy(name, spath);
			}
			return 0;
		}
	}

	return -1;
}

int
login_tty(int fd)
{
	setsid();
	if (ioctl(fd, TIOCSCTTY, NULL) < 0) {
		return -1;
	}
	dup2(fd, STDIN_FILENO);
	dup2(fd, STDOUT_FILENO);
	dup2(fd, STDERR_FILENO);
	if (fd > STDERR_FILENO) {
		close(fd);
	}
	return 0;
}

pid_t
forkpty(int *amaster, char *name, struct termios *termp, struct winsize *winp)
{
	int master, slave;
	pid_t pid;

	if (openpty(&master, &slave, name, termp, winp) < 0) {
		return -1;
	}

	pid = fork();
	if (pid < 0) {
		close(master);
		close(slave);
		return -1;
	}

	if (pid == 0) {
		close(master);
		if (login_tty(slave) < 0) {
			_exit(1);
		}
		return 0;
	}

	*amaster = master;
	close(slave);
	return pid;
}
