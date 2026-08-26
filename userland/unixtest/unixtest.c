/* Proof that AF_UNIX domain sockets actually work end to end -- needed for
 * X11's default transport (/tmp/.X11-unix/X0), previously just raw syscall
 * wrappers in libc with the header comment's own "aspirational, not yet
 * exercised" caveat (see TODO.md's X11 milestone). Two real processes (a
 * fork()'d parent/child, not a single-process both-ends shortcut like
 * networktest.c's loopback test): the parent bind()s+listen()s on a real
 * filesystem path and accept()s; the child connect()s to that same path.
 * Both directions of send()/recv() are exercised, plus SOCK_DGRAM
 * sendto()/recvfrom() over a second path, so both socket types AF_UNIX
 * actually needs get real coverage.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#define STREAM_PATH "/tmp/unixtest_stream.sock"
#define DGRAM_PATH  "/tmp/unixtest_dgram.sock"

static int
run_stream_test(void)
{
	unlink(STREAM_PATH);

	int lfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (lfd < 0) {
		printf("UNIXTEST FAIL: stream socket() failed, errno=%d\n", errno);
		return 1;
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, STREAM_PATH, sizeof(addr.sun_path) - 1);
	addr.sun_len = (unsigned char)(sizeof(addr) - sizeof(addr.sun_path) + strlen(addr.sun_path));

	if (bind(lfd, (struct sockaddr *)&addr, addr.sun_len) != 0) {
		printf("UNIXTEST FAIL: bind(%s) failed, errno=%d\n", STREAM_PATH, errno);
		return 1;
	}
	if (listen(lfd, 1) != 0) {
		printf("UNIXTEST FAIL: listen failed, errno=%d\n", errno);
		return 1;
	}
	printf("UNIXTEST: bind+listen on %s ok\n", STREAM_PATH);

	pid_t pid = fork();
	if (pid < 0) {
		printf("UNIXTEST FAIL: fork failed, errno=%d\n", errno);
		return 1;
	}

	if (pid == 0) {
		/* Child: connect, send a request, read the reply. */
		close(lfd);
		int cfd = socket(AF_UNIX, SOCK_STREAM, 0);
		if (cfd < 0) {
			printf("UNIXTEST FAIL (child): socket() failed, errno=%d\n", errno);
			_exit(1);
		}
		if (connect(cfd, (struct sockaddr *)&addr, addr.sun_len) != 0) {
			printf("UNIXTEST FAIL (child): connect() failed, errno=%d\n", errno);
			_exit(1);
		}
		const char * req = "ping from child";
		if (send(cfd, req, strlen(req), 0) != (ssize_t)strlen(req)) {
			printf("UNIXTEST FAIL (child): send() failed, errno=%d\n", errno);
			_exit(1);
		}
		char buf[64];
		memset(buf, 0, sizeof(buf));
		ssize_t n = recv(cfd, buf, sizeof(buf) - 1, 0);
		if (n <= 0) {
			printf("UNIXTEST FAIL (child): recv() failed, errno=%d\n", errno);
			_exit(1);
		}
		if (strcmp(buf, "pong from parent") != 0) {
			printf("UNIXTEST FAIL (child): reply mismatch: '%s'\n", buf);
			_exit(1);
		}
		printf("UNIXTEST: child got correct reply\n");
		close(cfd);
		_exit(0);
	}

	/* Parent: accept the child's connection, read its request, reply. */
	int afd = accept(lfd, (struct sockaddr *)0, (socklen_t *)0);
	if (afd < 0) {
		printf("UNIXTEST FAIL: accept() failed, errno=%d\n", errno);
		return 1;
	}
	char buf[64];
	memset(buf, 0, sizeof(buf));
	ssize_t n = recv(afd, buf, sizeof(buf) - 1, 0);
	if (n <= 0) {
		printf("UNIXTEST FAIL: recv() from child failed, errno=%d\n", errno);
		return 1;
	}
	if (strcmp(buf, "ping from child") != 0) {
		printf("UNIXTEST FAIL: request mismatch: '%s'\n", buf);
		return 1;
	}
	const char * reply = "pong from parent";
	if (send(afd, reply, strlen(reply), 0) != (ssize_t)strlen(reply)) {
		printf("UNIXTEST FAIL: send() reply failed, errno=%d\n", errno);
		return 1;
	}

	int status = 0;
	waitpid(pid, &status, 0);
	close(afd);
	close(lfd);
	unlink(STREAM_PATH);

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		printf("UNIXTEST FAIL: child exited abnormally (status=0x%x)\n", status);
		return 1;
	}

	printf("UNIXTEST: real bind/listen/accept/connect/send/recv round trip (SOCK_STREAM) OK\n");
	return 0;
}

static int
run_dgram_test(void)
{
	unlink(DGRAM_PATH);

	int sfd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (sfd < 0) {
		printf("UNIXTEST FAIL: dgram socket() failed, errno=%d\n", errno);
		return 1;
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, DGRAM_PATH, sizeof(addr.sun_path) - 1);
	addr.sun_len = (unsigned char)(sizeof(addr) - sizeof(addr.sun_path) + strlen(addr.sun_path));

	if (bind(sfd, (struct sockaddr *)&addr, addr.sun_len) != 0) {
		printf("UNIXTEST FAIL: dgram bind(%s) failed, errno=%d\n", DGRAM_PATH, errno);
		return 1;
	}

	/* Single process is fine for a connectionless round trip -- sendto()
	 * itself, over a real filesystem-namespace address, is the thing
	 * under test, same reasoning networktest.c's own UDP test gives. */
	const char * msg = "datagram payload";
	if (sendto(sfd, msg, strlen(msg), 0, (struct sockaddr *)&addr, addr.sun_len) != (ssize_t)strlen(msg)) {
		printf("UNIXTEST FAIL: dgram sendto() failed, errno=%d\n", errno);
		return 1;
	}

	char buf[64];
	memset(buf, 0, sizeof(buf));
	struct sockaddr_un from;
	socklen_t fromlen = sizeof(from);
	ssize_t n = recvfrom(sfd, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&from, &fromlen);
	if (n <= 0) {
		printf("UNIXTEST FAIL: dgram recvfrom() failed, errno=%d\n", errno);
		return 1;
	}
	if (strcmp(buf, msg) != 0) {
		printf("UNIXTEST FAIL: dgram payload mismatch: '%s'\n", buf);
		return 1;
	}

	close(sfd);
	unlink(DGRAM_PATH);
	printf("UNIXTEST: real sendto/recvfrom round trip (SOCK_DGRAM) OK\n");
	return 0;
}

int
main(void)
{
	if (run_stream_test() != 0) {
		return 1;
	}
	if (run_dgram_test() != 0) {
		return 1;
	}
	printf("UNIXTEST PASS\n");
	return 0;
}
