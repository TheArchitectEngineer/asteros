/* The _IOx-style macros and TIOC* numbers below are ground-truthed against
 * src/xnu/bsd/sys/ioccom.h and src/xnu/bsd/sys/ttycom.h. */
#ifndef _SYS_IOCTL_H_
#define _SYS_IOCTL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/ioccom.h> /* _IOC/_IO/_IOR/_IOW/_IOWR, IOC_VOID et al */
#include <sys/filio.h> /* FIONBIO et al -- real Darwin's sys/ioctl.h
                         * includes this unconditionally too; added for the
                         * X11 milestone (xterm's main.c uses FIONBIO with
                         * only sys/ioctl.h included, matching real Apple's
                         * header layout). */

struct winsize {
	unsigned short ws_row;
	unsigned short ws_col;
	unsigned short ws_xpixel;
	unsigned short ws_ypixel;
};

struct termios; /* see termios.h */

#define TIOCGETA   _IOR('t', 19, struct termios)
#define TIOCSETA   _IOW('t', 20, struct termios)
#define TIOCSETAW  _IOW('t', 21, struct termios)
#define TIOCSETAF  _IOW('t', 22, struct termios)
#define TIOCGWINSZ _IOR('t', 104, struct winsize)
#define TIOCSWINSZ _IOW('t', 103, struct winsize)
#define TIOCGPGRP  _IOR('t', 119, int)
#define TIOCSPGRP  _IOW('t', 118, int)
#define TIOCSCTTY  _IO('t', 97)
#define FIONREAD   _IOR('f', 127, int)

/* km(4) console text-renderer on/off -- KMIOCDISABLCONS is real Apple
 * (bsd/dev/kmreg_com.h, KERNEL_PRIVATE there so not directly usable
 * from userland); KMIOCENABLCONS is a project-local addition (see
 * that header's comment -- this project has no IOKit driver to ever
 * call the real re-enable path, so needed an explicit one). Added for
 * the X11 milestone (hw/kdrive/fbdev/asteros_input.c uses these to
 * stop the kernel's own console text -- printf, boot self-test daemon
 * output, tty echo -- from being drawn over Xfbdev's framebuffer). */
#define KMIOCENABLCONS  _IO('k', 7)
#define KMIOCDISABLCONS _IO('k', 8)

int ioctl(int fd, unsigned long request, ...);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_IOCTL_H_ */
