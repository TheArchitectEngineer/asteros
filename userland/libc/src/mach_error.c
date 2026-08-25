/* See mach/mach_error.h -- numeric-only stand-in for real Darwin's
 * symbolic kern_return_t decoder, used only for diagnostic logging. */
#include <mach/mach_error.h>
#include <mach/mach_msg_destroy.h>
#include <stdio.h>

const char *
mach_error_string(kern_return_t error_value)
{
	static char buf[32];
	snprintf(buf, sizeof(buf), "kern_return_t %d", error_value);
	return buf;
}

/* See mach/mach_msg_destroy.h -- documented v1 no-op. */
void
mach_msg_destroy(mach_msg_header_t *msg)
{
	(void)msg;
}
