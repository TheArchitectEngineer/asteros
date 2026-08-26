/* Proof that /dev/psevent (bsd/dev/i386/psevent.c) actually delivers real
 * PS/2 keyboard and mouse events to userspace -- opens the device
 * non-blocking, polls for up to PSTEST_BUDGET_MS total wall-clock time,
 * printing every event as it arrives (type, code/button, up/down,
 * dx/dy). Verification is live and external: this test doesn't inject
 * its own input (nothing on this OS can synthesize real i8042 traffic
 * from userspace), so real PS/2 packets have to come from actual
 * keypresses/mouse motion -- during interactive verification that means
 * the QEMU monitor's `sendkey`/`mouse_move`/`mouse_button` commands,
 * which inject through the same emulated i8042 hardware path as a real
 * keyboard/mouse would. Prints PSTEST PASS once it has seen at least one
 * of each event type (key, motion, button); PSTEST TIMEOUT otherwise,
 * with whatever it did see printed so a live boot session can eyeball
 * partial results.
 */
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <psevent.h>

#define PSTEST_BUDGET_MS 60000
#define PSTEST_POLL_MS   100

int
main(void)
{
	int fd = open("/dev/psevent", O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		printf("PSTEST FAIL: open(/dev/psevent) failed\n");
		return 1;
	}

	int saw_key = 0, saw_motion = 0, saw_button = 0;
	int elapsed_ms = 0;

	while (elapsed_ms < PSTEST_BUDGET_MS) {
		struct ps2_event ev;
		ssize_t n = read(fd, &ev, sizeof(ev));
		if (n == (ssize_t)sizeof(ev)) {
			switch (ev.type) {
			case PSEVENT_TYPE_KEY:
				printf("PSTEST: KEY code=0x%02x %s\n", ev.code, ev.value ? "down" : "up");
				saw_key = 1;
				break;
			case PSEVENT_TYPE_MOTION:
				printf("PSTEST: MOTION dx=%d dy=%d\n", ev.dx, ev.dy);
				saw_motion = 1;
				break;
			case PSEVENT_TYPE_BUTTON:
				printf("PSTEST: BUTTON %d %s\n", ev.code, ev.value ? "down" : "up");
				saw_button = 1;
				break;
			default:
				printf("PSTEST: unknown event type=%d\n", ev.type);
				break;
			}
			if (saw_key && saw_motion && saw_button) {
				printf("PSTEST PASS\n");
				return 0;
			}
			continue; /* drain without sleeping while events are flowing */
		}
		usleep(PSTEST_POLL_MS * 1000);
		elapsed_ms += PSTEST_POLL_MS;
	}

	printf("PSTEST TIMEOUT: key=%d motion=%d button=%d\n", saw_key, saw_motion, saw_button);
	return 1;
}
