/* Userland-side copy of the /dev/psevent wire format -- kept in sync by
 * hand with src/xnu/bsd/dev/i386/psevent.h, same pattern every other
 * kernel/userland ABI boundary in this tree uses (this libc has no
 * mechanism to #include a kernel header directly). This is this
 * project's own protocol, not a system ABI -- see the kernel header's
 * comment for why. */
#ifndef _PSEVENT_H_
#define _PSEVENT_H_

#include <stdint.h>

#define PSEVENT_TYPE_KEY    1
#define PSEVENT_TYPE_MOTION 2
#define PSEVENT_TYPE_BUTTON 3

#define PSEVENT_BUTTON_LEFT   0
#define PSEVENT_BUTTON_RIGHT  1
#define PSEVENT_BUTTON_MIDDLE 2

struct ps2_event {
	uint8_t  type;
	uint8_t  code;
	uint8_t  value;
	uint8_t  _pad;
	int32_t  dx;
	int32_t  dy;
};

#endif /* _PSEVENT_H_ */
