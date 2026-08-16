/* bootstrap_register()/bootstrap_look_up(): the client half of this
 * project's own bootstrap-namespace protocol -- a real named-service
 * registry, served by launchd (userland/launchd/bootstrap_server.c), that
 * replaces the old "one implicit service per process tree" hack
 * (task_get/set_special_port(TASK_BOOTSTRAP_PORT) used directly as the
 * service port -- see userland/libxpc/xpc_connection.c's prior history).
 *
 * Hand-marshaled in exactly mach_special_ports.c's style (own NDR record,
 * own request/reply structs, mach_msg_overwrite() with a separate reply
 * buffer to avoid the same-buffer clobber hazard) -- but, unlike
 * task_get/set_special_port, there is no real xnu-generated server header
 * to ground-truth field offsets against: real Darwin's bootstrap protocol
 * is served by launchd in userland, not by a kernel MIG subsystem, and
 * this project doesn't attempt to replicate Apple's actual (and
 * version-jumbled: bootstrap_register/register2/look_up/look_up2/look_up3)
 * wire format. Same "our own design, nothing outside this OS's own
 * process pairs ever needs to decode it" precedent already used for
 * libxpc's TLV wire format -- both sides of this protocol (this file, and
 * bootstrap_server.c's dispatch loop) just need to agree with each other.
 *
 * bootstrap_register's request is COMPLEX (Head + msgh_body + one
 * mach_msg_port_descriptor_t, disposition COPY_SEND -- the caller keeps
 * its own receive right and hands launchd a copy of a send right it
 * already minted, same "receiver mints its own send right" idiom
 * mach_port_insert_right's callers everywhere else in this tree already
 * use) + NDR + a fixed-size name field; its reply is always the plain
 * mig_simple_reply shape (Head+NDR+RetCode), matching
 * task_set_special_port's reply.
 *
 * bootstrap_look_up's request is SIMPLE (Head+NDR+name only); its reply is
 * a union of the success shape (Head+msgh_body+one port descriptor,
 * disposition COPY_SEND) or the plain error shape -- the same
 * success/error union pattern task_get_special_port's reply already uses.
 */
#include <mach/kern_return.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/mach_traps.h>
#include <mach/message.h>
#include <mach/bootstrap.h>
#include <mach/bootstrap_priv.h>
#include <mach/ndr.h>
#include <mach/port.h>
#include <string.h>

/* Same "every field but the four encoding ones is reserved-zero, and
 * those four are also 0 for this target" NDR record mach_special_ports.c
 * already documents and embeds its own copy of. */
static const NDR_record_t kNDRRecord = {0, 0, 0, 0, 0, 0, 0, 0};

kern_return_t
bootstrap_register(mach_port_t bp, const char *name, mach_port_t service_port)
{
	struct bootstrap_register_request req;
	struct bootstrap_mig_simple_reply reply;
	mach_port_name_t reply_port = mach_reply_port();

	req.Head.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE)
	    | MACH_MSGH_BITS_COMPLEX;
	req.Head.msgh_size = (mach_msg_size_t)sizeof(req);
	req.Head.msgh_remote_port = bp;
	req.Head.msgh_local_port = (mach_port_t)reply_port;
	req.Head.msgh_voucher_port = MACH_PORT_NULL;
	req.Head.msgh_id = BOOTSTRAP_REGISTER_MSGH_ID;
	req.msgh_body.msgh_descriptor_count = 1;
	req.service_port.name = service_port;
	req.service_port.pad1 = 0;
	req.service_port.pad2 = 0;
	req.service_port.disposition = MACH_MSG_TYPE_COPY_SEND;
	req.service_port.type = MACH_MSG_PORT_DESCRIPTOR;
	req.NDR = kNDRRecord;
	strncpy(req.name, name, sizeof(req.name) - 1);
	req.name[sizeof(req.name) - 1] = '\0';

	mach_msg_return_t mr = mach_msg_overwrite(&req.Head, MACH_SEND_MSG | MACH_RCV_MSG,
	    (mach_msg_size_t)sizeof(req), (mach_msg_size_t)(sizeof(reply) + MAX_TRAILER_SIZE),
	    reply_port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL,
	    &reply.Head, (mach_msg_size_t)(sizeof(reply) + MAX_TRAILER_SIZE));

	mach_port_deallocate(mach_task_self(), reply_port);

	if (mr != MACH_MSG_SUCCESS) {
		return (kern_return_t)mr;
	}
	if (reply.Head.msgh_id != BOOTSTRAP_REGISTER_MSGH_ID + 100) {
		return KERN_FAILURE;
	}
	return reply.RetCode;
}

kern_return_t
bootstrap_look_up(mach_port_t bp, const char *name, mach_port_t *service_port)
{
	struct bootstrap_look_up_request req;
	union bootstrap_look_up_reply reply;
	mach_port_name_t reply_port = mach_reply_port();

	req.Head.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
	req.Head.msgh_size = (mach_msg_size_t)sizeof(req);
	req.Head.msgh_remote_port = bp;
	req.Head.msgh_local_port = (mach_port_t)reply_port;
	req.Head.msgh_voucher_port = MACH_PORT_NULL;
	req.Head.msgh_id = BOOTSTRAP_LOOK_UP_MSGH_ID;
	req.NDR = kNDRRecord;
	strncpy(req.name, name, sizeof(req.name) - 1);
	req.name[sizeof(req.name) - 1] = '\0';

	mach_msg_return_t mr = mach_msg_overwrite(&req.Head, MACH_SEND_MSG | MACH_RCV_MSG,
	    (mach_msg_size_t)sizeof(req), (mach_msg_size_t)sizeof(reply),
	    reply_port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL,
	    &reply.success.Head, (mach_msg_size_t)sizeof(reply));

	mach_port_deallocate(mach_task_self(), reply_port);

	if (mr != MACH_MSG_SUCCESS) {
		return (kern_return_t)mr;
	}
	if (reply.error.Head.msgh_id != BOOTSTRAP_LOOK_UP_MSGH_ID + 100) {
		return KERN_FAILURE;
	}
	if (!(reply.error.Head.msgh_bits & MACH_MSGH_BITS_COMPLEX)) {
		/* Simple reply -- the error shape (name not registered). */
		return reply.error.RetCode;
	}

	*service_port = reply.success.service_port.name;
	return KERN_SUCCESS;
}
