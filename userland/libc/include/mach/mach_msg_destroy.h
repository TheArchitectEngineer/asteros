/* Real mach_msg_destroy() walks a Mach message's descriptors, releasing
 * any embedded port rights and deallocating any out-of-line VM regions
 * that weren't otherwise consumed -- real libsyscall's cleanup path for
 * a message that failed to send or errored out complex. This project's
 * only caller (Phase 25's configd_server.c, adapted from real Apple
 * source) only ever reaches it on already-rare error paths (a reply
 * send that failed, or an incoming request the demux rejected) -- a
 * documented v1 no-op rather than a real descriptor walk: worst case is
 * a leaked port right or small VM allocation on one of those paths,
 * never on this project's own successful request/reply round trips. */
#ifndef _MACH_MACH_MSG_DESTROY_H_
#define _MACH_MACH_MSG_DESTROY_H_

#include <mach/message.h>

#ifdef __cplusplus
extern "C" {
#endif

void mach_msg_destroy(mach_msg_header_t *msg);

#ifdef __cplusplus
}
#endif

#endif /* _MACH_MACH_MSG_DESTROY_H_ */
