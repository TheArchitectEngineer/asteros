/* Offline proof of Phase 30's libresolv v1 surface: this project has no
 * live NIC yet (same documented limitation as Phase 24's own networking
 * work), so an actual round trip to a real nameserver can't be tested --
 * instead this exercises the real, vendored wire-format code
 * deterministically, entirely offline:
 *
 *   1. res_mkquery() builds a real DNS query packet for "www.example.com"
 *      and the result is checked byte-for-byte against what a compliant
 *      query must look like (header flags/counts, checked via the raw
 *      RFC 1035 header layout -- HEADER's bitfield struct is
 *      res_private.h-internal, not part of the public API, so this
 *      reads the same fixed byte offsets ns_get16()/real DNS wire format
 *      define instead).
 *   2. ns_initparse()/ns_parserr() (the real message parser) reads that
 *      same packet back and recovers the question name/type/class.
 *   3. A hand-built synthetic DNS *response* (header + echoed question +
 *      one A-record answer using a compression pointer back into the
 *      question, the way a real server's response would) is fed through
 *      ns_initparse()/ns_parserr() again, proving compression-pointer
 *      expansion and answer-RR decoding both work on real vendored code
 *      -- not just the encode half.
 *
 * Same pattern as userland/SystemConfiguration/test/sctest.c: a normal
 * dynamically-linked executable, PASS/FAIL to stdout.
 */
#include <arpa/nameser.h>
#include <resolv.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond, msg) \
	do { \
		if (!(cond)) { \
			printf("RESTEST FAIL: %s\n", msg); \
			exit(1); \
		} \
	} while (0)

/* Raw RFC 1035 header field offsets/bits -- avoids res_private.h's
 * internal HEADER bitfield struct (not part of the public API, and
 * bitfield layout is itself endianness-sensitive). */
#define DNS_HDR_QR_OPCODE_BYTE(buf)	((buf)[2])
#define DNS_HDR_QDCOUNT(buf)		ns_get16((buf) + 4)
#define DNS_HDR_ANCOUNT(buf)		ns_get16((buf) + 6)

int
main(void)
{
	u_char qbuf[512];
	int qlen;
	ns_msg handle;
	ns_rr rr;

	/* 1. Build a real query. */
	qlen = res_mkquery(ns_o_query, "www.example.com", ns_c_in, ns_t_a,
	    NULL, 0, NULL, qbuf, sizeof(qbuf));
	CHECK(qlen > NS_HFIXEDSZ, "res_mkquery returned too little data");

	unsigned char qr_opcode = DNS_HDR_QR_OPCODE_BYTE(qbuf);
	CHECK((qr_opcode & 0x80) == 0, "query header QR bit should be 0 (a query, not a response)");
	CHECK(((qr_opcode >> 3) & 0x0F) == ns_o_query, "query header opcode should be QUERY");
	CHECK(DNS_HDR_QDCOUNT(qbuf) == 1, "query header qdcount should be 1");
	CHECK(DNS_HDR_ANCOUNT(qbuf) == 0, "query header ancount should be 0");

	/* 2. Parse that same query back with the real message parser. */
	CHECK(ns_initparse(qbuf, qlen, &handle) == 0, "ns_initparse on our own query failed");
	CHECK(ns_msg_count(handle, ns_s_qd) == 1, "parsed question count should be 1");
	CHECK(ns_parserr(&handle, ns_s_qd, 0, &rr) == 0, "ns_parserr on the question failed");
	CHECK(strcasecmp(ns_rr_name(rr), "www.example.com") == 0, "parsed question name mismatch");
	CHECK(ns_rr_type(rr) == ns_t_a, "parsed question type should be A");
	CHECK(ns_rr_class(rr) == ns_c_in, "parsed question class should be IN");

	/* 3. Hand-build a synthetic response: header + the same question
	 * (reusing res_mkquery's own encoded QNAME/QTYPE/QCLASS bytes, which
	 * start right after the 12-byte header) + one A answer whose name is
	 * a compression pointer back to offset 12 (where that question
	 * starts), real TTL/RDLENGTH/RDATA fields, and a real 4-byte A
	 * record payload (203.0.113.7, a TEST-NET-3 address -- RFC 5737). */
	u_char rbuf[512];
	size_t off = 0;

	memset(rbuf, 0, NS_HFIXEDSZ);
	rbuf[0] = qbuf[0]; rbuf[1] = qbuf[1]; /* echo the same ID */
	rbuf[2] = 0x80; /* QR=1, opcode=QUERY(0) */
	rbuf[3] = 0x00; /* RCODE=NOERROR */
	rbuf[4] = 0x00; rbuf[5] = 0x01; /* QDCOUNT=1 */
	rbuf[6] = 0x00; rbuf[7] = 0x01; /* ANCOUNT=1 */
	/* NSCOUNT/ARCOUNT already zeroed above */
	off = NS_HFIXEDSZ;

	size_t qsec_len = (size_t)qlen - NS_HFIXEDSZ;
	memcpy(rbuf + off, qbuf + NS_HFIXEDSZ, qsec_len);
	off += qsec_len;

	/* Answer RR: name = compression pointer to offset 12 (0xC00C). */
	rbuf[off++] = 0xC0;
	rbuf[off++] = 0x0C;
	rbuf[off++] = (ns_t_a >> 8) & 0xFF;
	rbuf[off++] = ns_t_a & 0xFF;
	rbuf[off++] = (ns_c_in >> 8) & 0xFF;
	rbuf[off++] = ns_c_in & 0xFF;
	u_int32_t ttl = htonl(300);
	memcpy(rbuf + off, &ttl, 4);
	off += 4;
	rbuf[off++] = 0x00;
	rbuf[off++] = 0x04; /* RDLENGTH = 4 */
	struct in_addr addr;
	CHECK(inet_pton(AF_INET, "203.0.113.7", &addr) == 1, "inet_pton setup failed");
	memcpy(rbuf + off, &addr, 4);
	off += 4;

	CHECK(ns_initparse(rbuf, (int)off, &handle) == 0, "ns_initparse on synthetic response failed");
	CHECK(ns_msg_count(handle, ns_s_an) == 1, "synthetic response answer count should be 1");
	CHECK(ns_parserr(&handle, ns_s_an, 0, &rr) == 0, "ns_parserr on the answer failed");
	CHECK(strcasecmp(ns_rr_name(rr), "www.example.com") == 0,
	    "answer name (via compression pointer) mismatch");
	CHECK(ns_rr_type(rr) == ns_t_a, "answer type should be A");
	CHECK(ns_rr_class(rr) == ns_c_in, "answer class should be IN");
	CHECK(ns_rr_rdlen(rr) == 4, "answer rdlength should be 4");
	CHECK(memcmp(ns_rr_rdata(rr), &addr, 4) == 0, "answer A record bytes mismatch");

	char ascii[INET_ADDRSTRLEN];
	CHECK(inet_ntop(AF_INET, ns_rr_rdata(rr), ascii, sizeof(ascii)) != NULL, "inet_ntop failed");
	CHECK(strcmp(ascii, "203.0.113.7") == 0, "decoded answer address mismatch");

	printf("RESTEST PASS\n");
	return 0;
}
