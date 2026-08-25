/* End-to-end proof of Phase 25's SystemConfiguration.framework v1
 * surface: a real SCDynamicStoreCreate() -> SetValue() -> CopyValue()
 * round trip against the real, vendored configd (over real generated
 * MIG stubs, config.defs), plus a real async notification test --
 * SetNotificationKeys() + NotifyFileDescriptor() on one session,
 * SetValue() on a second session, then poll()/read() proving the first
 * session's fd genuinely becomes readable, not just that values can be
 * get/set synchronously. Same pattern as userland/Security/test/
 * securitytest.c -- a normal dynamically-linked executable against
 * libSystemConfiguration.dylib + libCoreFoundation.dylib +
 * libSystem.B.dylib.
 */
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCDynamicStoreCopySpecific.h>
#include <SystemConfiguration/SCPrivate.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(cond, msg) \
	do { \
		if (!(cond)) { \
			printf("SCTEST FAIL: %s\n", msg); \
			exit(1); \
		} \
	} while (0)

int
main(void)
{
	SCDynamicStoreRef	writer;
	SCDynamicStoreRef	reader;
	CFStringRef		key;
	CFStringRef		value;
	CFStringRef		readBack;
	CFArrayRef		keys;
	int			notifyFD;
	struct pollfd		pfd;
	int			pret;
	int32_t			identifier;
	ssize_t			n;

	/*
	 * This project's launchd (Phase 28) doesn't implement on-demand
	 * launch (bootstrap_check_in-style lazy service start, see
	 * configd_server.c's own header comment) -- so unlike real Darwin,
	 * where a client's first bootstrap_look_up() would block until
	 * configd finishes registering, here it just fails immediately if
	 * configd hasn't called bootstrap_register() yet. RunAtLoad gives
	 * no ordering/readiness guarantee between daemons, so retry with a
	 * short grace period instead of assuming configd already won the
	 * race.
	 */
	writer = NULL;
	for (int attempt = 0; attempt < 40 && writer == NULL; attempt++) {
		if (attempt > 0) usleep(250000);
		writer = SCDynamicStoreCreate(NULL, CFSTR("sctest-writer"), NULL, NULL);
	}
	CHECK(writer != NULL, "SCDynamicStoreCreate(writer)");

	reader = SCDynamicStoreCreate(NULL, CFSTR("sctest-reader"), NULL, NULL);
	CHECK(reader != NULL, "SCDynamicStoreCreate(reader)");

	key = SCDynamicStoreKeyCreate(NULL, CFSTR("%@"), CFSTR("com.asteros.sctest/State"));
	CHECK(key != NULL, "SCDynamicStoreKeyCreate");

	value = CFSTR("hello from sctest");
	CHECK(SCDynamicStoreSetValue(writer, key, value), "SCDynamicStoreSetValue #1");

	readBack = (CFStringRef)SCDynamicStoreCopyValue(writer, key);
	CHECK(readBack != NULL, "SCDynamicStoreCopyValue");
	CHECK(CFEqual(readBack, value), "round-tripped value equals what was set");
	CFRelease(readBack);

	printf("SCTEST: real get/set round trip OK\n");

	keys = CFArrayCreate(NULL, (const void **)&key, 1, &kCFTypeArrayCallBacks);
	CHECK(keys != NULL, "CFArrayCreate(keys)");
	CHECK(SCDynamicStoreSetNotificationKeys(reader, keys, NULL), "SCDynamicStoreSetNotificationKeys");
	CFRelease(keys);

	notifyFD = -1;
	CHECK(SCDynamicStoreNotifyFileDescriptor(reader, 0, &notifyFD), "SCDynamicStoreNotifyFileDescriptor");
	CHECK(notifyFD >= 0, "notifyFD valid");

	CHECK(SCDynamicStoreSetValue(writer, key, CFSTR("changed!")), "SCDynamicStoreSetValue #2");

	pfd.fd = notifyFD;
	pfd.events = POLLIN;
	pfd.revents = 0;
	pret = poll(&pfd, 1, 5000);
	CHECK(pret == 1, "poll() woke up on notification");
	CHECK((pfd.revents & POLLIN) != 0, "notifyFD is readable");

	n = read(notifyFD, &identifier, sizeof(identifier));
	CHECK(n == (ssize_t)sizeof(identifier), "read() notification identifier");

	printf("SCTEST: real async notification wakeup OK (identifier=%d)\n", identifier);

	close(notifyFD);
	CHECK(SCDynamicStoreRemoveValue(writer, key), "SCDynamicStoreRemoveValue");
	CFRelease(key);
	CFRelease(writer);
	CFRelease(reader);

	printf("SCTEST PASS\n");
	return 0;
}
