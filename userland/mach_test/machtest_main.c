/* End-to-end proof that userland Mach IPC is real (Phase 21): a genuine
 * mach_msg() send/receive round trip between two independent processes,
 * with the receiving port handed from parent to child via the
 * TASK_BOOTSTRAP_PORT special-port mechanism -- not an arbitrary shared
 * port name. This deliberately exercises the exact mechanism the rest of
 * the SystemConfiguration port depends on (Phase 25's configd needs
 * every client process to reach it via its own inherited bootstrap
 * port), rather than a simpler but less representative IPC test.
 *
 * Ordinary mach_port_allocate()'d ports do NOT propagate across fork() --
 * ground-truthed by reading src/xnu/osfmk/kern/ipc_tt.c's
 * ipc_task_init(): every new task (fork included) gets a brand-new,
 * empty IPC space; only the small set of *special* ports (self, host,
 * bootstrap, exception ports, ...) are explicitly copied down from the
 * parent. So the test:
 *   1. Parent allocates a RECEIVE right, derives a SEND right at the
 *      same name (mach_port_insert_right(..., MAKE_SEND)) -- the
 *      standard "receiver mints its own send right" idiom.
 *   2. Parent installs that send right as its own TASK_BOOTSTRAP_PORT
 *      (task_set_special_port) -- consumes the send right, keeps the
 *      receive right.
 *   3. fork(). The child's itk_bootstrap is a real kernel-side copy of
 *      the parent's (ipc_task_init, non-NULL parent branch) -- so the
 *      child can task_get_special_port(TASK_BOOTSTRAP_PORT) and get a
 *      valid send right to the parent's receive port, with zero shared
 *      state beyond what the kernel itself propagates.
 *   4. Child sends a one-word message carrying a payload, with its own
 *      mach_reply_port() as the reply port.
 *   5. Parent receives it, replies with a derived payload.
 *   6. Child receives the reply and checks it -- MACHTEST PASS iff the
 *      full send/receive/reply round trip produced the right bytes.
 *
 * Real bug found and fixed live in QEMU while building this: every
 * receive failed with MACH_RCV_TOO_LARGE (0x10004004) because `rcv_size`
 * covered only sizeof(header)+sizeof(payload), leaving no room for the
 * trailer (security/audit metadata, MACH_MSG_TRAILER_FORMAT_0) the
 * kernel always appends after the message body on receive -- initially
 * misread as MACH_RCV_TIMED_OUT (0x10004003, one less) before checking
 * the exact constants in message.h. Fixed by padding the receive buffer
 * with MAX_TRAILER_SIZE and keeping `send_size` limited to the real
 * header+payload bytes (MACHTEST_SEND_SIZE below) so the padding is never
 * actually transmitted. mach_special_ports.c's MIG replies already
 * accounted for this; this struct didn't.
 */
#include <mach/mach.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/mach_traps.h>
#include <mach/message.h>
#include <mach/task_special_ports.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

#define MACHTEST_MSGH_ID 0x4d544553 /* 'MTES' */
#define MACHTEST_PAYLOAD  0x600df00d

struct machtest_msg {
	mach_msg_header_t Head;
	int payload;
	char trailer_pad[128]; /* room for the kernel-appended receive trailer --
	                         * see the file header comment. Never sent, only
	                         * ever used as headroom for rcv_size. */
};

#define MACHTEST_SEND_SIZE ((mach_msg_size_t)offsetof(struct machtest_msg, trailer_pad))

int
main(void)
{
	mach_port_name_t recv_name;
	kern_return_t kr;

	kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &recv_name);
	if (kr != KERN_SUCCESS) {
		printf("MACHTEST FAIL: mach_port_allocate kr=%d\n", kr);
		return 1;
	}

	kr = mach_port_insert_right(mach_task_self(), recv_name, recv_name, MACH_MSG_TYPE_MAKE_SEND);
	if (kr != KERN_SUCCESS) {
		printf("MACHTEST FAIL: mach_port_insert_right kr=%d\n", kr);
		return 1;
	}

	kr = task_set_special_port(mach_task_self(), TASK_BOOTSTRAP_PORT, recv_name);
	if (kr != KERN_SUCCESS) {
		printf("MACHTEST FAIL: task_set_special_port kr=%d\n", kr);
		return 1;
	}

	pid_t pid = fork();
	if (pid < 0) {
		printf("MACHTEST FAIL: fork errno set\n");
		return 1;
	}

	if (pid == 0) {
		/* Child: fetch the inherited bootstrap send right, send a
		 * request, wait for the reply. */
		mach_port_t send_port = MACH_PORT_NULL;
		kr = task_get_special_port(mach_task_self(), TASK_BOOTSTRAP_PORT, &send_port);
		if (kr != KERN_SUCCESS) {
			printf("MACHTEST FAIL (child): task_get_special_port kr=%d\n", kr);
			_exit(1);
		}

		mach_port_name_t reply_port = mach_reply_port();

		struct machtest_msg req;
		req.Head.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
		req.Head.msgh_size = MACHTEST_SEND_SIZE;
		req.Head.msgh_remote_port = send_port;
		req.Head.msgh_local_port = (mach_port_t)reply_port;
		req.Head.msgh_voucher_port = MACH_PORT_NULL;
		req.Head.msgh_id = MACHTEST_MSGH_ID;
		req.payload = MACHTEST_PAYLOAD;

		kr = (kern_return_t)mach_msg(&req.Head, MACH_SEND_MSG, MACHTEST_SEND_SIZE,
		    0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
		if (kr != KERN_SUCCESS) {
			printf("MACHTEST FAIL (child): send kr=%d\n", kr);
			_exit(1);
		}

		struct machtest_msg reply;
		kr = (kern_return_t)mach_msg(&reply.Head, MACH_RCV_MSG, 0, (mach_msg_size_t)sizeof(reply),
		    reply_port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
		if (kr != KERN_SUCCESS) {
			printf("MACHTEST FAIL (child): receive kr=%d\n", kr);
			_exit(1);
		}

		if (reply.payload != (int)(MACHTEST_PAYLOAD ^ 0x1)) {
			printf("MACHTEST FAIL (child): bad reply payload 0x%x\n", reply.payload);
			_exit(1);
		}

		printf("MACHTEST PASS\n");
		_exit(0);
	}

	/* Parent: receive the child's request on the retained receive
	 * right, reply with a derived payload. */
	struct machtest_msg req;
	kr = (kern_return_t)mach_msg(&req.Head, MACH_RCV_MSG, 0, (mach_msg_size_t)sizeof(req),
	    recv_name, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
	if (kr != KERN_SUCCESS) {
		printf("MACHTEST FAIL (parent): receive kr=%d\n", kr);
		int status;
		waitpid(pid, &status, 0);
		return 1;
	}

	struct machtest_msg reply;
	reply.Head.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE, 0);
	reply.Head.msgh_size = MACHTEST_SEND_SIZE;
	reply.Head.msgh_remote_port = req.Head.msgh_remote_port; /* the reply-once right. Empirically
	                                                           * verified live in QEMU (printed both
	                                                           * fields after a real receive): the
	                                                           * kernel SWAPS field roles on delivery
	                                                           * -- the sender's msgh_local_port
	                                                           * (reply-to) is exposed to the
	                                                           * receiver as msgh_remote_port, and
	                                                           * the sender's msgh_remote_port
	                                                           * (destination) becomes the
	                                                           * receiver's msgh_local_port (the
	                                                           * port it received on). An earlier
	                                                           * reading of ipc_kmsg_copyout_header's
	                                                           * internal `reply = msg->msgh_local_port`
	                                                           * local variable was a real but
	                                                           * misleading clue -- that's the
	                                                           * pre-remap wire value, not the final
	                                                           * field userland actually sees. */
	reply.Head.msgh_local_port = MACH_PORT_NULL;
	reply.Head.msgh_voucher_port = MACH_PORT_NULL;
	reply.Head.msgh_id = MACHTEST_MSGH_ID + 1;
	reply.payload = req.payload ^ 0x1;

	kr = (kern_return_t)mach_msg(&reply.Head, MACH_SEND_MSG, MACHTEST_SEND_SIZE,
	    0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
	if (kr != KERN_SUCCESS) {
		printf("MACHTEST FAIL (parent): reply send kr=%d\n", kr);
	}

	int status = 0;
	waitpid(pid, &status, 0);
	return 0;
}
