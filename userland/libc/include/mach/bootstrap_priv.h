/*
 * Wire-format structs for this project's own bootstrap-namespace protocol
 * -- shared between the one client implementation
 * (userland/libc/src/mach_bootstrap.c, mach_bootstrap.h's
 * bootstrap_register()/bootstrap_look_up()) and the one server
 * implementation (userland/launchd/bootstrap_server.c). Not part of
 * mach/bootstrap.h's public API -- real callers only ever see the two
 * function declarations there, never these struct layouts directly.
 */
#ifndef _MACH_BOOTSTRAP_PRIV_H_
#define _MACH_BOOTSTRAP_PRIV_H_

#include <mach/bootstrap.h>
#include <mach/message.h>
#include <mach/ndr.h>

struct bootstrap_register_request {
	mach_msg_header_t Head;
	mach_msg_body_t msgh_body;
	mach_msg_port_descriptor_t service_port;
	NDR_record_t NDR;
	char name[BOOTSTRAP_MAX_NAME_LEN];
};

struct bootstrap_mig_simple_reply {
	mach_msg_header_t Head;
	NDR_record_t NDR;
	kern_return_t RetCode;
};

struct bootstrap_look_up_request {
	mach_msg_header_t Head;
	NDR_record_t NDR;
	char name[BOOTSTRAP_MAX_NAME_LEN];
};

struct bootstrap_look_up_reply_success {
	mach_msg_header_t Head;
	mach_msg_body_t msgh_body;
	mach_msg_port_descriptor_t service_port;
};

union bootstrap_look_up_reply {
	struct bootstrap_look_up_reply_success success;
	struct bootstrap_mig_simple_reply error;
	char pad[sizeof(struct bootstrap_look_up_reply_success) + MAX_TRAILER_SIZE];
};

#endif /* _MACH_BOOTSTRAP_PRIV_H_ */
