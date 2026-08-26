/* mig_get_reply_port()/mig_dealloc_reply_port()/mig_put_reply_port():
 * every real generated MIG client stub (configUser.c, Phase 25) calls
 * these three to obtain the one-shot reply port each RPC waits its
 * reply on. Real Darwin's libsyscall caches one reply port per thread
 * (a fresh mach_reply_port() is comparatively expensive to mint, and a
 * stale one is safe to reuse across calls from the same thread) via
 * _pthread_getspecific -- same real recipe here, using this project's
 * own real pthread_getspecific/pthread_setspecific (Phase 16) instead
 * of libpthread's internal variant.
 *
 * mig_put_reply_port() is a no-op in the simple (non-cthreads) case
 * real Darwin also ships -- the port just stays cached for reuse by the
 * next call on this thread. mig_dealloc_reply_port() is called instead
 * when a stub got back a "wrong reply" error, forcing a fresh port to
 * be minted next time.
 */
#include <mach/mig.h>
#include <mach/mach_traps.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <pthread.h>

static pthread_once_t	replyPortKeyOnce	= PTHREAD_ONCE_INIT;
static pthread_key_t	replyPortKey;

static void
initReplyPortKey(void)
{
	pthread_key_create(&replyPortKey, NULL);
}

mach_port_t
mig_get_reply_port(void)
{
	mach_port_t	port;

	pthread_once(&replyPortKeyOnce, initReplyPortKey);

	port = (mach_port_t)(uintptr_t)pthread_getspecific(replyPortKey);
	if (port == MACH_PORT_NULL) {
		port = (mach_port_t)mach_reply_port();
		pthread_setspecific(replyPortKey, (void *)(uintptr_t)port);
	}

	return port;
}

void
mig_dealloc_reply_port(mach_port_t reply_port)
{
	pthread_once(&replyPortKeyOnce, initReplyPortKey);

	if (reply_port != MACH_PORT_NULL) {
		mach_port_mod_refs(mach_task_self(), reply_port, MACH_PORT_RIGHT_RECEIVE, -1);
	}
	pthread_setspecific(replyPortKey, (void *)(uintptr_t)MACH_PORT_NULL);
}

void
mig_put_reply_port(mach_port_t reply_port)
{
	(void)reply_port;
	/* simple case: leave the port cached for the next call on this thread */
}

/* No voucher_mach_msg_set() stub here: config.defs' generated stubs are
 * now produced with real mig's -novouchers flag (see
 * userland/toolchain/mig/gen_config_defs.sh), matching real Apple's own
 * build practice for non-kernel Mach interfaces (ground-truthed against
 * this project's own vendored src/xnu/libsyscall/xcodescripts/
 * mach_install_mig.sh, which passes -novouchers on every one of its real
 * mig invocations) -- this suppresses the "BEGIN/END VOUCHER CODE"
 * codegen block entirely, so nothing ever references this symbol. */
