/* End-to-end proof that the real TCP/IP stack works: assigns 127.0.0.1/8
 * to lo0 (compiled in via the NETWORKING_DEV config bundle, but never
 * exercised from userland before Phase 24 -- see TODO.md), then drives a
 * real AF_INET SOCK_STREAM listen/connect/accept round trip and a real
 * SOCK_DGRAM sendto/recvfrom round trip over it. Single process, no
 * fork(): a loopback listen()er's three-way handshake completes inside
 * the kernel as soon as connect() is called (nothing here has to race
 * calling accept() first), so both ends can live in one process, same as
 * userland/pthread_test/pthread_test_main.c avoids unnecessary process
 * complexity for a self-contained test.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

#define TCP_PORT 15001
#define UDP_PORT_A 15002
#define UDP_PORT_B 15003

static int
bring_up_loopback(void)
{
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		printf("NETWORKTEST FAIL: socket() for ioctl failed: %d\n", errno);
		return -1;
	}

	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, "lo0", IFNAMSIZ - 1);

	struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);
	inet_pton(AF_INET, "127.0.0.1", &sin->sin_addr);
	if (ioctl(s, SIOCSIFADDR, &ifr) < 0) {
		printf("NETWORKTEST FAIL: SIOCSIFADDR(lo0, 127.0.0.1) failed: %d\n", errno);
		close(s);
		return -1;
	}

	memset(&ifr.ifr_addr, 0, sizeof(ifr.ifr_addr));
	sin = (struct sockaddr_in *)&ifr.ifr_netmask;
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);
	inet_pton(AF_INET, "255.0.0.0", &sin->sin_addr);
	if (ioctl(s, SIOCSIFNETMASK, &ifr) < 0) {
		printf("NETWORKTEST FAIL: SIOCSIFNETMASK(lo0, 255.0.0.0) failed: %d\n", errno);
		close(s);
		return -1;
	}

	memset(&ifr.ifr_addr, 0, sizeof(ifr.ifr_addr));
	if (ioctl(s, SIOCGIFFLAGS, &ifr) < 0) {
		printf("NETWORKTEST FAIL: SIOCGIFFLAGS(lo0) failed: %d\n", errno);
		close(s);
		return -1;
	}
	ifr.ifr_flags |= IFF_UP;
	if (ioctl(s, SIOCSIFFLAGS, &ifr) < 0) {
		printf("NETWORKTEST FAIL: SIOCSIFFLAGS(lo0, up) failed: %d\n", errno);
		close(s);
		return -1;
	}

	close(s);
	printf("NETWORKTEST: lo0 up, 127.0.0.1/8\n");
	return 0;
}

static int
tcp_roundtrip(void)
{
	int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	int client_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0 || client_fd < 0) {
		printf("NETWORKTEST FAIL: TCP socket() failed\n");
		return -1;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_len = sizeof(addr);
	addr.sin_port = htons(TCP_PORT);
	inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

	int reuse = 1;
	setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

	if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		printf("NETWORKTEST FAIL: TCP bind() failed: %d\n", errno);
		return -1;
	}
	if (listen(listen_fd, 1) < 0) {
		printf("NETWORKTEST FAIL: TCP listen() failed: %d\n", errno);
		return -1;
	}
	if (connect(client_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		printf("NETWORKTEST FAIL: TCP connect() failed: %d\n", errno);
		return -1;
	}

	struct sockaddr_in peer;
	socklen_t peerlen = sizeof(peer);
	int conn_fd = accept(listen_fd, (struct sockaddr *)&peer, &peerlen);
	if (conn_fd < 0) {
		printf("NETWORKTEST FAIL: TCP accept() failed: %d\n", errno);
		return -1;
	}

	const char *ping = "ping";
	char buf[16];
	memset(buf, 0, sizeof(buf));

	if (send(client_fd, ping, strlen(ping), 0) != (ssize_t)strlen(ping)) {
		printf("NETWORKTEST FAIL: TCP client send() failed\n");
		return -1;
	}
	ssize_t n = recv(conn_fd, buf, sizeof(buf) - 1, 0);
	if (n <= 0 || strcmp(buf, ping) != 0) {
		printf("NETWORKTEST FAIL: TCP server recv() got %.*s (n=%ld), want %s\n",
		    (int)n, buf, (long)n, ping);
		return -1;
	}

	const char *pong = "pong";
	memset(buf, 0, sizeof(buf));
	if (send(conn_fd, pong, strlen(pong), 0) != (ssize_t)strlen(pong)) {
		printf("NETWORKTEST FAIL: TCP server send() failed\n");
		return -1;
	}
	n = recv(client_fd, buf, sizeof(buf) - 1, 0);
	if (n <= 0 || strcmp(buf, pong) != 0) {
		printf("NETWORKTEST FAIL: TCP client recv() got %.*s (n=%ld), want %s\n",
		    (int)n, buf, (long)n, pong);
		return -1;
	}

	close(conn_fd);
	close(client_fd);
	close(listen_fd);
	printf("NETWORKTEST: TCP loopback round trip (listen/connect/accept/send/recv) OK\n");
	return 0;
}

static int
udp_roundtrip(void)
{
	int fd_a = socket(AF_INET, SOCK_DGRAM, 0);
	int fd_b = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd_a < 0 || fd_b < 0) {
		printf("NETWORKTEST FAIL: UDP socket() failed\n");
		return -1;
	}

	struct sockaddr_in addr_a, addr_b;
	memset(&addr_a, 0, sizeof(addr_a));
	addr_a.sin_family = AF_INET;
	addr_a.sin_len = sizeof(addr_a);
	addr_a.sin_port = htons(UDP_PORT_A);
	inet_pton(AF_INET, "127.0.0.1", &addr_a.sin_addr);

	memset(&addr_b, 0, sizeof(addr_b));
	addr_b.sin_family = AF_INET;
	addr_b.sin_len = sizeof(addr_b);
	addr_b.sin_port = htons(UDP_PORT_B);
	inet_pton(AF_INET, "127.0.0.1", &addr_b.sin_addr);

	if (bind(fd_a, (struct sockaddr *)&addr_a, sizeof(addr_a)) < 0) {
		printf("NETWORKTEST FAIL: UDP bind(A) failed: %d\n", errno);
		return -1;
	}
	if (bind(fd_b, (struct sockaddr *)&addr_b, sizeof(addr_b)) < 0) {
		printf("NETWORKTEST FAIL: UDP bind(B) failed: %d\n", errno);
		return -1;
	}

	const char *msg = "udp-ping";
	if (sendto(fd_b, msg, strlen(msg), 0, (struct sockaddr *)&addr_a, sizeof(addr_a))
	    != (ssize_t)strlen(msg)) {
		printf("NETWORKTEST FAIL: UDP sendto(B->A) failed\n");
		return -1;
	}

	char buf[32];
	memset(buf, 0, sizeof(buf));
	struct sockaddr_in from;
	socklen_t fromlen = sizeof(from);
	ssize_t n = recvfrom(fd_a, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&from, &fromlen);
	if (n <= 0 || strcmp(buf, msg) != 0) {
		printf("NETWORKTEST FAIL: UDP recvfrom(A) got %.*s (n=%ld), want %s\n",
		    (int)n, buf, (long)n, msg);
		return -1;
	}
	if (from.sin_port != htons(UDP_PORT_B)) {
		printf("NETWORKTEST FAIL: UDP sender port %d, want %d\n",
		    ntohs(from.sin_port), UDP_PORT_B);
		return -1;
	}

	const char *reply = "udp-pong";
	if (sendto(fd_a, reply, strlen(reply), 0, (struct sockaddr *)&from, fromlen)
	    != (ssize_t)strlen(reply)) {
		printf("NETWORKTEST FAIL: UDP sendto(A->B) failed\n");
		return -1;
	}
	memset(buf, 0, sizeof(buf));
	n = recvfrom(fd_b, buf, sizeof(buf) - 1, 0, NULL, NULL);
	if (n <= 0 || strcmp(buf, reply) != 0) {
		printf("NETWORKTEST FAIL: UDP recvfrom(B) got %.*s (n=%ld), want %s\n",
		    (int)n, buf, (long)n, reply);
		return -1;
	}

	close(fd_a);
	close(fd_b);
	printf("NETWORKTEST: UDP loopback round trip (sendto/recvfrom, sender addr verified) OK\n");
	return 0;
}

int
main(void)
{
	if (bring_up_loopback() != 0) {
		return 1;
	}
	if (tcp_roundtrip() != 0) {
		return 1;
	}
	if (udp_roundtrip() != 0) {
		return 1;
	}
	printf("NETWORKTEST PASS\n");
	return 0;
}
