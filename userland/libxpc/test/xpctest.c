/* Copyright (c) 2026 Vihaan Nathan
 *
 * End-to-end proof that libxpc is real: a genuine xpc_connection_t
 * client<->service round trip between two independent processes (fork(),
 * same shape as userland/mach_test/machtest_main.c) plus a same-process
 * object-model self-check (retain/copy/equal, nested dictionary/array).
 *
 * The parent is the listener (a real Mach receive right installed as its
 * own TASK_BOOTSTRAP_PORT, exactly machtest's bootstrap mechanism -- see
 * xpc_connection.c). It forks; the child is the client, inheriting a send
 * right to the parent's listener port automatically via the kernel's
 * ipc_task_init(). The child sends a dictionary (string, int64, and a
 * nested array of strings) with xpc_connection_send_message_with_reply_
 * sync(), the parent's per-peer event handler replies via
 * xpc_dictionary_create_reply()/xpc_connection_send_message(), and the
 * child verifies every field came back correctly transformed -- a real
 * cross-process encode -> Mach IPC send -> decode -> handler ->
 * encode -> Mach IPC send -> decode round trip, not a loopback shortcut.
 * A second, fire-and-forget xpc_connection_send_message() (no reply)
 * proves the async delivery path independently of the reply-correlation
 * path: the parent's handler flips a flag the parent's own main()
 * observes after waitpid(), off the real dispatch_async_f() delivery a
 * background worker thread drains, not the receive thread itself.
 *
 * Real named multi-service bootstrap lookup (mach/bootstrap.h,
 * userland/launchd/bootstrap_server.c) replaced the old single-ambient-
 * port mechanism this file's history refers to above -- proven here by
 * registering a *second*, independently-named listener
 * ("com.asteros.xpctest.second") with an observably different reply
 * (value*3 + "pong2" instead of value*2 + "pong") alongside the first,
 * and having the child look both up by name and get back two genuinely
 * different connections. A third lookup against a name that was never
 * registered ("com.asteros.xpctest.nonexistent") confirms a failed
 * lookup doesn't silently resolve to *any* service -- no reply ever
 * arrives, checked with the same bounded-poll idiom as the oneway
 * notify check below.
 */
#include <xpc/xpc.h>
#include <dispatch/dispatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static volatile int g_got_oneway = 0;
static volatile int g_got_nonexistent_reply = 0;

static void
test_object_model(void)
{
	xpc_object_t items = xpc_array_create_empty();
	xpc_object_t s1 = xpc_string_create("alpha");
	xpc_object_t s2 = xpc_string_create("beta");
	xpc_array_append_value(items, s1);
	xpc_array_append_value(items, s2);
	xpc_release(s1);
	xpc_release(s2);

	xpc_object_t dict = xpc_dictionary_create_empty();
	xpc_dictionary_set_string(dict, "name", "widget");
	xpc_dictionary_set_int64(dict, "count", -7);
	xpc_dictionary_set_uint64(dict, "flags", 0xff);
	xpc_dictionary_set_double(dict, "ratio", 1.5);
	xpc_dictionary_set_bool(dict, "on", true);
	xpc_dictionary_set_data(dict, "blob", "\x01\x02\x03", 3);
	xpc_dictionary_set_value(dict, "items", items);
	xpc_release(items);

	xpc_object_t copy = xpc_copy(dict);
	if (!xpc_equal(dict, copy)) {
		printf("XPCTEST FAIL: xpc_copy/xpc_equal mismatch on a nested dictionary\n");
		exit(1);
	}
	if (xpc_dictionary_get_int64(copy, "count") != -7) {
		printf("XPCTEST FAIL: xpc_copy lost a scalar field\n");
		exit(1);
	}
	xpc_object_t copy_items = xpc_dictionary_get_array(copy, "items");
	if (!copy_items || xpc_array_get_count(copy_items) != 2 ||
	    strcmp(xpc_string_get_string_ptr(xpc_array_get_value(copy_items, 1)), "beta") != 0) {
		printf("XPCTEST FAIL: xpc_copy lost the nested array\n");
		exit(1);
	}
	/* mutate the copy, confirm the original is untouched -- proves this
	 * is a real deep copy, not a shared reference. */
	xpc_dictionary_set_int64(copy, "count", 99);
	if (xpc_dictionary_get_int64(dict, "count") != -7) {
		printf("XPCTEST FAIL: xpc_copy is not independent of the original\n");
		exit(1);
	}

	xpc_release(dict);
	xpc_release(copy);
}

static void
service_handle_event(xpc_object_t event)
{
	if (xpc_get_type(event) != XPC_TYPE_DICTIONARY) {
		return;
	}
	const char *cmd = xpc_dictionary_get_string(event, "cmd");
	if (!cmd) {
		return;
	}
	if (strcmp(cmd, "ping") == 0) {
		int64_t value = xpc_dictionary_get_int64(event, "value");
		xpc_object_t in_items = xpc_dictionary_get_array(event, "items");

		xpc_object_t reply = xpc_dictionary_create_reply(event);
		xpc_dictionary_set_string(reply, "response", "pong");
		xpc_dictionary_set_int64(reply, "value", value * 2);
		if (in_items) {
			xpc_object_t out_items = xpc_copy(in_items);
			xpc_dictionary_set_value(reply, "items", out_items);
			xpc_release(out_items);
		}
		xpc_connection_send_message(xpc_dictionary_get_remote_connection(event), reply);
		xpc_release(reply);
	} else if (strcmp(cmd, "notify") == 0) {
		g_got_oneway = 1;
	}
}

static void
run_service(void)
{
	xpc_connection_t listener = xpc_connection_create_mach_service(
	    "com.asteros.xpctest", NULL, XPC_CONNECTION_MACH_SERVICE_LISTENER);

	xpc_connection_set_event_handler(listener, ^(xpc_object_t peer_obj) {
		if (xpc_get_type(peer_obj) != XPC_TYPE_CONNECTION) {
			return;
		}
		xpc_connection_t peer = peer_obj;
		xpc_connection_set_event_handler(peer, ^(xpc_object_t event) {
			service_handle_event(event);
		});
		xpc_connection_activate(peer);
	});
	xpc_connection_activate(listener);
}

/* A second, independently-named service registered against the same
 * shared launchd registry -- its handler is deliberately observably
 * different (value*3 + "pong2") from service_handle_event()'s (value*2 +
 * "pong") so the client can tell the two names apart by their replies,
 * not just by "a reply arrived at all". */
static void
service_handle_event_second(xpc_object_t event)
{
	if (xpc_get_type(event) != XPC_TYPE_DICTIONARY) {
		return;
	}
	const char *cmd = xpc_dictionary_get_string(event, "cmd");
	if (!cmd || strcmp(cmd, "ping") != 0) {
		return;
	}
	int64_t value = xpc_dictionary_get_int64(event, "value");
	xpc_object_t reply = xpc_dictionary_create_reply(event);
	xpc_dictionary_set_string(reply, "response", "pong2");
	xpc_dictionary_set_int64(reply, "value", value * 3);
	xpc_connection_send_message(xpc_dictionary_get_remote_connection(event), reply);
	xpc_release(reply);
}

static void
run_service_second(void)
{
	xpc_connection_t listener = xpc_connection_create_mach_service(
	    "com.asteros.xpctest.second", NULL, XPC_CONNECTION_MACH_SERVICE_LISTENER);

	xpc_connection_set_event_handler(listener, ^(xpc_object_t peer_obj) {
		if (xpc_get_type(peer_obj) != XPC_TYPE_CONNECTION) {
			return;
		}
		xpc_connection_t peer = peer_obj;
		xpc_connection_set_event_handler(peer, ^(xpc_object_t event) {
			service_handle_event_second(event);
		});
		xpc_connection_activate(peer);
	});
	xpc_connection_activate(listener);
}

static int
run_client(void)
{
	xpc_connection_t client = xpc_connection_create_mach_service("com.asteros.xpctest", NULL, 0);
	xpc_connection_set_event_handler(client, ^(xpc_object_t event) {
		(void)event; /* unused for this test -- the client only expects replies */
	});
	xpc_connection_activate(client);

	xpc_object_t items = xpc_array_create_empty();
	xpc_object_t s1 = xpc_string_create("alpha");
	xpc_object_t s2 = xpc_string_create("beta");
	xpc_array_append_value(items, s1);
	xpc_array_append_value(items, s2);
	xpc_release(s1);
	xpc_release(s2);

	xpc_object_t msg = xpc_dictionary_create_empty();
	xpc_dictionary_set_string(msg, "cmd", "ping");
	xpc_dictionary_set_int64(msg, "value", 21);
	xpc_dictionary_set_value(msg, "items", items);
	xpc_release(items);

	xpc_object_t reply = xpc_connection_send_message_with_reply_sync(client, msg);
	xpc_release(msg);

	if (!reply || xpc_get_type(reply) != XPC_TYPE_DICTIONARY) {
		printf("XPCTEST FAIL (child): no dictionary reply\n");
		return 1;
	}
	const char *resp = xpc_dictionary_get_string(reply, "response");
	if (!resp || strcmp(resp, "pong") != 0) {
		printf("XPCTEST FAIL (child): bad response string %s\n", resp ? resp : "(null)");
		return 1;
	}
	if (xpc_dictionary_get_int64(reply, "value") != 42) {
		printf("XPCTEST FAIL (child): bad echoed value %lld\n",
		    (long long)xpc_dictionary_get_int64(reply, "value"));
		return 1;
	}
	xpc_object_t ritems = xpc_dictionary_get_array(reply, "items");
	if (!ritems || xpc_array_get_count(ritems) != 2 ||
	    strcmp(xpc_string_get_string_ptr(xpc_array_get_value(ritems, 0)), "alpha") != 0 ||
	    strcmp(xpc_string_get_string_ptr(xpc_array_get_value(ritems, 1)), "beta") != 0) {
		printf("XPCTEST FAIL (child): bad echoed items array\n");
		return 1;
	}
	xpc_release(reply);

	/* Second, independently-named service: proves the name actually
	 * resolves to a *different* port through the shared registry, not
	 * the same one every lookup used to return under the old single-
	 * ambient-slot mechanism. */
	xpc_connection_t client2 = xpc_connection_create_mach_service("com.asteros.xpctest.second", NULL, 0);
	xpc_connection_set_event_handler(client2, ^(xpc_object_t event) {
		(void)event;
	});
	xpc_connection_activate(client2);

	xpc_object_t msg2 = xpc_dictionary_create_empty();
	xpc_dictionary_set_string(msg2, "cmd", "ping");
	xpc_dictionary_set_int64(msg2, "value", 21);
	xpc_object_t reply2 = xpc_connection_send_message_with_reply_sync(client2, msg2);
	xpc_release(msg2);

	if (!reply2 || xpc_get_type(reply2) != XPC_TYPE_DICTIONARY) {
		printf("XPCTEST FAIL (child): second service gave no dictionary reply\n");
		return 1;
	}
	const char *resp2 = xpc_dictionary_get_string(reply2, "response");
	if (!resp2 || strcmp(resp2, "pong2") != 0) {
		printf("XPCTEST FAIL (child): second service bad response string %s\n", resp2 ? resp2 : "(null)");
		return 1;
	}
	if (xpc_dictionary_get_int64(reply2, "value") != 63) {
		printf("XPCTEST FAIL (child): second service bad echoed value %lld\n",
		    (long long)xpc_dictionary_get_int64(reply2, "value"));
		return 1;
	}
	xpc_release(reply2);

	/* A name that was never registered anywhere: bootstrap_look_up()
	 * fails, xpc_connection_activate() logs and returns without starting
	 * a receive thread, and no reply -- from this or any other service --
	 * should ever arrive. Uses the async send + bounded poll (not the
	 * sync variant) since a lookup failure leaves nothing to ever wake a
	 * blocking wait. */
	xpc_connection_t client3 = xpc_connection_create_mach_service(
	    "com.asteros.xpctest.nonexistent", NULL, 0);
	xpc_connection_set_event_handler(client3, ^(xpc_object_t event) {
		(void)event;
		g_got_nonexistent_reply = 1;
	});
	xpc_connection_activate(client3);

	xpc_object_t msg3 = xpc_dictionary_create_empty();
	xpc_dictionary_set_string(msg3, "cmd", "ping");
	xpc_connection_send_message_with_reply(client3, msg3, NULL, ^(xpc_object_t event) {
		(void)event;
		g_got_nonexistent_reply = 1;
	});
	xpc_release(msg3);

	for (int i = 0; i < 10; i++) {
		usleep(20000);
	}
	if (g_got_nonexistent_reply) {
		printf("XPCTEST FAIL (child): got a reply from a name that was never registered\n");
		return 1;
	}

	xpc_object_t notif = xpc_dictionary_create_empty();
	xpc_dictionary_set_string(notif, "cmd", "notify");
	xpc_connection_send_message(client, notif);
	xpc_release(notif);

	return 0;
}

int
main(void)
{
	test_object_model();

	run_service();
	run_service_second();

	pid_t pid = fork();
	if (pid < 0) {
		printf("XPCTEST FAIL: fork errno set\n");
		return 1;
	}

	if (pid == 0) {
		int rc = run_client();
		if (rc != 0) {
			_exit(1);
		}
		printf("XPCTEST PASS (child side)\n");
		_exit(0);
	}

	int status = 0;
	waitpid(pid, &status, 0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		printf("XPCTEST FAIL: client process failed\n");
		return 1;
	}

	for (int i = 0; i < 100 && !g_got_oneway; i++) {
		usleep(20000);
	}
	if (!g_got_oneway) {
		printf("XPCTEST FAIL: service never observed the fire-and-forget notify\n");
		return 1;
	}

	printf("XPCTEST PASS\n");
	return 0;
}
