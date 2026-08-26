/* Real RFC 1706 NSAP address presentation format: "0x" followed by the
 * address in hex, with a "." inserted after every second byte. Added for
 * Phase 30 (real vendored libresolv's ns_print.c uses this only to print
 * NSAP DNS records for debugging -- no crypto or proprietary logic here,
 * just a fixed, public text-encoding spec). Original implementation of
 * that spec, not derived from any vendored source file. */
#include <arpa/inet.h>
#include <stddef.h>

static char nsap_ntoa_buf[3 + 255 * 3];

char *
inet_nsap_ntoa(int binlen, const unsigned char *binary, char *ascii)
{
	char *tp = ascii ? ascii : nsap_ntoa_buf;
	char *start = tp;
	int i;

	if (binlen > 255)
		binlen = 255;
	if (binlen < 0)
		binlen = 0;

	*tp++ = '0';
	*tp++ = 'x';

	for (i = 0; i < binlen; i++) {
		static const char hex[] = "0123456789abcdef";
		unsigned char b = binary[i];

		*tp++ = hex[b >> 4];
		*tp++ = hex[b & 0x0f];
		if ((i % 2) == 1 && i < binlen - 1)
			*tp++ = '.';
	}
	*tp = '\0';

	return start;
}
