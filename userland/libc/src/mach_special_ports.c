/* Hand-marshaled MIG client stubs for task_get_special_port/
 * task_set_special_port -- the two routines Phase 21's bootstrap design
 * needs (task_special_ports.h's task_get_bootstrap_port/
 * task_set_bootstrap_port macros expand directly to these). No `mig`
 * tool output exists anywhere in this tree, so the wire structs below
 * were ground-truthed field-by-field against the kernel's own generated
 * server stub -- src/xnu/BUILD/obj/DEVELOPMENT_X86_64/osfmk/DEVELOPMENT/
 * mach/task_server.{c,h} -- not guessed or copied from a header. Same
 * "ground-truth against the real compiled artifact" discipline this
 * project already used for objc_abi.h (Phase 13) and struct termios
 * (Phase 9).
 *
 * task.defs declares `subsystem task 3400;` with task_get_special_port
 * and task_set_special_port as the 10th and 11th routines (0-indexed:
 * task_create, task_terminate, task_threads, mach_ports_register,
 * mach_ports_lookup, task_info, task_set_info, task_suspend, task_resume,
 * task_get_special_port, task_set_special_port) -> msgh_id 3409 and 3410,
 * confirmed directly against task_server.c's `__DeclareRcvRpc(3409, ...)`
 * / `__DeclareRcvRpc(3410, ...)`, not just computed from the subsystem
 * base.
 *
 * task_get_special_port request is SIMPLE (Head + NDR + which_port, no
 * port descriptor); its success reply is COMPLEX (Head + msgh_body +
 * one mach_msg_port_descriptor_t, disposition 17 == MACH_MSG_TYPE_MOVE_SEND
 * -- confirmed from task_server.c's `OutP->special_port.disposition = 17`
 * under `#if __MigKernelSpecificCode`, which is the branch actually
 * compiled into this kernel-side generated file). Its error reply is the
 * generic simple mig_reply_error_t (Head + NDR + RetCode) -- MIG always
 * falls back to this shape via MIG_RETURN_ERROR on any failure.
 *
 * task_set_special_port request is COMPLEX (Head + msgh_body + one
 * mach_msg_port_descriptor_t + NDR + which_port) and the descriptor's
 * disposition MUST be exactly 17 (MOVE_SEND) or the kernel's
 * __MIG_check__Request__task_set_special_port_t rejects it with
 * MIG_TYPE_ERROR -- confirmed from that exact check function. Its reply
 * (success or error) is always the simple mig_reply_error_t shape (no
 * port comes back).
 */
#include <mach/kern_return.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/mach_traps.h>
#include <mach/message.h>
#include <mach/ndr.h>
#include <mach/port.h>
#include <mach/task_special_ports.h>

#define TASK_GET_SPECIAL_PORT_MSGH_ID 3409
#define TASK_SET_SPECIAL_PORT_MSGH_ID 3410

/* Canonical NDR record for this target -- x86_64, little-endian, ASCII,
 * IEEE float -- ground-truthed against mach/i386/ndr_def.h's NDR_record
 * global (which every non-kernel image can't share directly since it's
 * defined `extern` per-image in real Darwin's libsyscall; each MIG
 * client TU conventionally embeds its own copy). Every field but
 * mig_encoding/int_rep/char_rep/float_rep is reserved-must-be-zero, and
 * those four are all also 0 for this target (NDR_PROTOCOL_2_0 ==
 * NDR_INT_LITTLE_ENDIAN == NDR_CHAR_ASCII == NDR_FLOAT_IEEE == 0). */
static const NDR_record_t kNDRRecord = {0, 0, 0, 0, 0, 0, 0, 0};

struct task_get_special_port_request {
	mach_msg_header_t Head;
	NDR_record_t NDR;
	int which_port;
};

struct task_get_special_port_reply_success {
	mach_msg_header_t Head;
	mach_msg_body_t msgh_body;
	mach_msg_port_descriptor_t special_port;
};

/* mig_reply_error_t shape (mach/mig_errors.h), used both for
 * task_get_special_port's error path and for every task_set_special_port
 * reply. */
struct mig_simple_reply {
	mach_msg_header_t Head;
	NDR_record_t NDR;
	kern_return_t RetCode;
};

union task_get_special_port_reply {
	struct task_get_special_port_reply_success success;
	struct mig_simple_reply error;
	char pad[sizeof(struct task_get_special_port_reply_success) + MAX_TRAILER_SIZE];
};

kern_return_t
task_get_special_port(mach_port_t task, int which_port, mach_port_t *special_port)
{
	struct task_get_special_port_request req;
	union task_get_special_port_reply reply;
	mach_port_name_t reply_port = mach_reply_port();

	req.Head.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
	req.Head.msgh_size = (mach_msg_size_t)sizeof(req);
	req.Head.msgh_remote_port = task;
	req.Head.msgh_local_port = (mach_port_t)reply_port;
	req.Head.msgh_voucher_port = MACH_PORT_NULL;
	req.Head.msgh_id = TASK_GET_SPECIAL_PORT_MSGH_ID;
	req.NDR = kNDRRecord;
	req.which_port = which_port;

	/* Distinct send (req) and receive (reply) buffers -- mach_msg()
	 * itself would reuse a single buffer for both (mach_msg_trap's
	 * rcv_msg=0 "same buffer" convention, see mach_msg.c), which would
	 * silently overwrite `req` instead of populating `reply`. Call
	 * mach_msg_overwrite() directly instead, exactly the case its
	 * separate rcv_msg parameter exists for. */
	mach_msg_return_t mr = mach_msg_overwrite(&req.Head, MACH_SEND_MSG | MACH_RCV_MSG,
	    (mach_msg_size_t)sizeof(req), (mach_msg_size_t)sizeof(reply),
	    reply_port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL,
	    &reply.success.Head, (mach_msg_size_t)sizeof(reply));

	mach_port_deallocate(mach_task_self(), reply_port);

	if (mr != MACH_MSG_SUCCESS) {
		return (kern_return_t)mr;
	}
	if (reply.error.Head.msgh_id != TASK_GET_SPECIAL_PORT_MSGH_ID + 100) {
		return KERN_FAILURE;
	}
	if (!(reply.error.Head.msgh_bits & MACH_MSGH_BITS_COMPLEX)) {
		/* Simple reply -- the error shape. */
		return reply.error.RetCode;
	}

	*special_port = reply.success.special_port.name;
	return KERN_SUCCESS;
}

struct task_set_special_port_request {
	mach_msg_header_t Head;
	mach_msg_body_t msgh_body;
	mach_msg_port_descriptor_t special_port;
	NDR_record_t NDR;
	int which_port;
};

kern_return_t
task_set_special_port(mach_port_t task, int which_port, mach_port_t special_port)
{
	struct task_set_special_port_request req;
	struct mig_simple_reply reply;
	mach_port_name_t reply_port = mach_reply_port();

	req.Head.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE)
	    | MACH_MSGH_BITS_COMPLEX;
	req.Head.msgh_size = (mach_msg_size_t)sizeof(req);
	req.Head.msgh_remote_port = task;
	req.Head.msgh_local_port = (mach_port_t)reply_port;
	req.Head.msgh_voucher_port = MACH_PORT_NULL;
	req.Head.msgh_id = TASK_SET_SPECIAL_PORT_MSGH_ID;
	req.msgh_body.msgh_descriptor_count = 1;
	req.special_port.name = special_port;
	req.special_port.pad1 = 0;
	req.special_port.pad2 = 0;
	req.special_port.disposition = MACH_MSG_TYPE_MOVE_SEND;
	req.special_port.type = MACH_MSG_PORT_DESCRIPTOR;
	req.NDR = kNDRRecord;
	req.which_port = which_port;

	/* Same same-buffer-clobber hazard as task_get_special_port above --
	 * use mach_msg_overwrite() with a distinct reply buffer. */
	mach_msg_return_t mr = mach_msg_overwrite(&req.Head, MACH_SEND_MSG | MACH_RCV_MSG,
	    (mach_msg_size_t)sizeof(req), (mach_msg_size_t)(sizeof(reply) + MAX_TRAILER_SIZE),
	    reply_port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL,
	    &reply.Head, (mach_msg_size_t)(sizeof(reply) + MAX_TRAILER_SIZE));

	mach_port_deallocate(mach_task_self(), reply_port);

	if (mr != MACH_MSG_SUCCESS) {
		return (kern_return_t)mr;
	}
	if (reply.Head.msgh_id != TASK_SET_SPECIAL_PORT_MSGH_ID + 100) {
		return KERN_FAILURE;
	}
	return reply.RetCode;
}
