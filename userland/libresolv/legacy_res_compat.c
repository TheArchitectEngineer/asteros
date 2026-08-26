/* AsterOS (Phase 30): resolv.h declares `extern struct __res_state
 * _res;` -- the classic, pre-thread-safe global resolver state real
 * ancient BSD code used directly -- but real vendored libresolv's own
 * modern implementation (res_data.c) only defines its OWN internal
 * `_res_9` singleton, never `_res` itself. Nothing in the 23 files this
 * project actually vendors calls into `_res` beyond referencing its
 * address, so a plain zero-initialized definition is enough to satisfy
 * the link -- matches every other "declared in a real header, no real
 * backing implementation needed for what this project actually
 * exercises" stub already used throughout this codebase.
 */
#include "resolv.h"
#include <sys/types.h>

struct __res_state _res;

/* AsterOS (Phase 30): ns_print.c's KEY-record debug printer calls
 * dst_s_dns_key_id() (macro-renamed to res_9_dst_s_dns_key_id) to show a
 * DNSSEC key's "key tag" -- a checksum computed purely for display, not
 * used in any cryptographic verification here (this project doesn't
 * vendor dst_*.c at all -- see build.sh's header comment). The
 * computation itself is the standard, public RFC 4034 Appendix B keytag
 * algorithm (not derived from Apple's or ISC's dst_support.c, which
 * this project doesn't vendor), so an original implementation of the
 * published spec is used here instead. */
u_int16_t
res_9_dst_s_dns_key_id(const u_char *key, const int keysize)
{
	unsigned long ac = 0;
	int i;

	for (i = 0; i < keysize; i++)
		ac += (i & 1) ? key[i] : ((unsigned long)key[i]) << 8;
	ac += (ac >> 16) & 0xffff;

	return (u_int16_t)(ac & 0xffff);
}
