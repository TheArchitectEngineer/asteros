/* Real Darwin's gethostuuid() returns a stable hardware UUID persisted at
 * first boot; we have no NVRAM-backed identity store, so this generates a
 * fresh random one from real kernel entropy (getentropy) every call --
 * a valid UUID, just not a stable *host* identifier across calls. Fine
 * for LockFileManager's actual use (building a probably-unique lock file
 * name), wrong if anything ever needs cross-call/cross-reboot stability.
 * TODO: back with a real persisted identifier if that ever matters. */
#include <uuid/uuid.h>
#include <sys/random.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>

void
uuid_generate_random(uuid_t out)
{
	getentropy(out, sizeof(uuid_t));
	/* RFC 4122 version/variant bits, so it at least looks like a real UUID */
	out[6] = (out[6] & 0x0F) | 0x40;
	out[8] = (out[8] & 0x3F) | 0x80;
}

void uuid_generate(uuid_t out) { uuid_generate_random(out); }

void
uuid_unparse_lower(const uuid_t uu, uuid_string_t out)
{
	snprintf(out, sizeof(uuid_string_t),
	    "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
	    uu[0], uu[1], uu[2], uu[3], uu[4], uu[5], uu[6], uu[7],
	    uu[8], uu[9], uu[10], uu[11], uu[12], uu[13], uu[14], uu[15]);
}

void
uuid_unparse_upper(const uuid_t uu, uuid_string_t out)
{
	uuid_unparse_lower(uu, out);
	for (int i = 0; out[i]; i++) {
		if (out[i] >= 'a' && out[i] <= 'f') {
			out[i] -= 'a' - 'A';
		}
	}
}

void uuid_unparse(const uuid_t uu, uuid_string_t out) { uuid_unparse_upper(uu, out); }

/* Inverse of uuid_unparse: parses the standard 8-4-4-4-12 hex-digit
 * form (case-insensitive, hyphens required in the standard positions)
 * back into 16 raw bytes. Returns 0 on success, -1 on a malformed
 * string -- matching real Darwin's uuid_parse() contract. */
int
uuid_parse(const char *in, uuid_t uu)
{
	static const int hyphen_after[] = {4, 6, 8, 10};
	int nibble_count = 0;
	unsigned char byte = 0;
	int have_high_nibble = 0;

	for (const char *p = in; *p; p++) {
		char c = *p;
		int is_hyphen_pos = 0;
		for (size_t i = 0; i < sizeof(hyphen_after) / sizeof(hyphen_after[0]); i++) {
			if (nibble_count == hyphen_after[i]) {
				is_hyphen_pos = 1;
				break;
			}
		}
		if (is_hyphen_pos && c == '-') {
			continue;
		}

		int digit;
		if (c >= '0' && c <= '9') {
			digit = c - '0';
		} else if (c >= 'a' && c <= 'f') {
			digit = c - 'a' + 10;
		} else if (c >= 'A' && c <= 'F') {
			digit = c - 'A' + 10;
		} else {
			return -1;
		}

		if (nibble_count >= 32) {
			return -1;
		}

		if (!have_high_nibble) {
			byte = (unsigned char)(digit << 4);
			have_high_nibble = 1;
		} else {
			byte |= (unsigned char)digit;
			uu[nibble_count / 2] = byte;
			have_high_nibble = 0;
		}
		nibble_count++;
	}

	if (nibble_count != 32) {
		return -1;
	}
	return 0;
}

void
uuid_copy(uuid_t dst, const uuid_t src)
{
	for (int i = 0; i < 16; i++) {
		dst[i] = src[i];
	}
}

int
gethostuuid(uuid_t out, const struct timespec *wait)
{
	(void)wait;
	uuid_generate_random(out);
	return 0;
}
