#ifndef _POLL_H_
#define _POLL_H_

/* Real Darwin's <poll.h> is just this -- sys/poll.h already has the
 * full struct pollfd/POLL* set. Ground-truthed against
 * src/xnu/bsd/sys/poll.h; this file previously duplicated a smaller,
 * incomplete subset (POLLRDNORM/POLLRDBAND/POLLWRNORM/POLLWRBAND were
 * missing), found live when WindowMaker's WINGs/handlers.c needed them.
 */
#include <sys/poll.h>

#endif /* _POLL_H_ */
