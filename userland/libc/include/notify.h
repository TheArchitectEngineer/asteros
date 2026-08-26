/* Minimal notify(3) shim -- real Darwin's lightweight cross-process
 * state/notification IPC (notifyd) doesn't exist in this project. Real
 * libresolv (Phase 30) calls a handful of these APIs to watch for
 * DNS-configuration-changed notifications and async-query cancellation,
 * always behind a check of the token it gets back: every call site
 * initializes its own token to -1 first and only proceeds past that
 * point if registration actually returned NOTIFY_STATUS_OK. Since
 * notify_register_plain()/notify_register_check() here always report
 * failure, callers permanently take the "no notification available"
 * path -- the same honest-inert-stub treatment already used elsewhere
 * in this project for subsystems with no real backing implementation
 * (e.g. notifyviaport/notifyviasignal, Phase 25).
 */
#ifndef _NOTIFY_H_
#define _NOTIFY_H_

#include <stdint.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

#define NOTIFY_STATUS_OK 0
#define NOTIFY_STATUS_FAILED 1

/* Well-known notification name components real libresolv builds a key
 * from; never resolved to anything real here. */
#define NOTIFY_DIR_NAME "com.apple.system.dns"
#define NOTIFY_DNS_CONTROL_NAME NOTIFY_DIR_NAME ".control"

uint32_t notify_register_check(const char *name, int *out_token);
uint32_t notify_register_plain(const char *name, int *out_token);
uint32_t notify_cancel(int token);
uint32_t notify_check(int token, int *check);
uint32_t notify_get_state(int token, uint64_t *state);
uint32_t notify_monitor_file(int token, const char *path, int flags);

__END_DECLS

#endif /* _NOTIFY_H_ */
