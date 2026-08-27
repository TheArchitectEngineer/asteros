/* Real Darwin declares openpty/login_tty/forkpty here (not in pty.h or a
 * separate libutil header) -- added for the X11 milestone (xterm's main.c
 * includes this unconditionally on __APPLE__ for openpty()). */
#ifndef _UTIL_H_
#define _UTIL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <termios.h>
#include <sys/ioctl.h> /* struct winsize -- see that header's own note about
                         * the (pre-existing) duplicate definition in
                         * sys/ttycom.h; only one of the two can be
                         * included into the same translation unit. */

int openpty(int *amaster, int *aslave, char *name, struct termios *termp, struct winsize *winp);
int login_tty(int fd);
pid_t forkpty(int *amaster, char *name, struct termios *termp, struct winsize *winp);

#ifdef __cplusplus
}
#endif

#endif /* _UTIL_H_ */
