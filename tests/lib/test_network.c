// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * lib/network.c unit tests: readn, writen, set_nonblocking, set_cloexec,
 * htonf/ntohf, frr_sequence_next/frr_sequence32_next
 */
#include <zebra.h>

#include <fcntl.h>

#include "network.h"

static int fail;

#define CHECK(cond, ...)                                                       \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL line %d: ", __LINE__);                    \
			printf(__VA_ARGS__);                                   \
			printf("\n");                                          \
			fail = 1;                                              \
		}                                                              \
	} while (0)

static void test_readn_writen(void)
{
	int fds[2];
	uint8_t out[64], in[64];
	size_t i;

	for (i = 0; i < sizeof(out); i++)
		out[i] = (uint8_t)i;
	memset(in, 0, sizeof(in));

	if (pipe(fds) < 0) {
		CHECK(0, "pipe failed");
		return;
	}

	CHECK(writen(fds[1], out, sizeof(out)) == (int)sizeof(out),
	      "writen full buffer");
	CHECK(readn(fds[0], in, sizeof(in)) == (int)sizeof(in),
	      "readn full buffer");
	CHECK(memcmp(in, out, sizeof(out)) == 0, "readn data mismatch");

	/* writer closes with fewer bytes than requested: short read */
	CHECK(writen(fds[1], out, 10) == 10, "writen 10 bytes");
	close(fds[1]);
	memset(in, 0, sizeof(in));
	CHECK(readn(fds[0], in, sizeof(in)) == 10, "readn short on EOF");
	CHECK(memcmp(in, out, 10) == 0, "short read data mismatch");

	/* EOF with no data */
	CHECK(readn(fds[0], in, sizeof(in)) == 0, "readn at EOF");
	close(fds[0]);

	/* read from invalid fd */
	CHECK(readn(-1, in, 1) < 0, "readn bad fd");
}

static void test_writen_errors(void)
{
	int fds[2];
	uint8_t byte = 1;

	CHECK(writen(-1, &byte, 1) < 0, "writen bad fd");

	if (pipe(fds) < 0) {
		CHECK(0, "pipe failed");
		return;
	}
	close(fds[0]);
	signal(SIGPIPE, SIG_IGN);
	CHECK(writen(fds[1], &byte, 1) < 0 && errno == EPIPE,
	      "writen to closed pipe");
	close(fds[1]);
}

static void test_fd_flags(void)
{
	int fds[2];
	int flags;

	if (pipe(fds) < 0) {
		CHECK(0, "pipe failed");
		return;
	}

	flags = fcntl(fds[0], F_GETFL);
	CHECK(!(flags & O_NONBLOCK), "pipe starts blocking");
	CHECK(set_nonblocking(fds[0]) == 0, "set_nonblocking ok");
	flags = fcntl(fds[0], F_GETFL);
	CHECK(flags & O_NONBLOCK, "O_NONBLOCK set");
	/* idempotent */
	CHECK(set_nonblocking(fds[0]) == 0, "set_nonblocking twice");

	flags = fcntl(fds[1], F_GETFD);
	CHECK(!(flags & FD_CLOEXEC), "pipe starts without cloexec");
	CHECK(set_cloexec(fds[1]) == 0, "set_cloexec ok");
	flags = fcntl(fds[1], F_GETFD);
	CHECK(flags & FD_CLOEXEC, "FD_CLOEXEC set");
	CHECK(set_cloexec(fds[1]) == 0, "set_cloexec twice");

	close(fds[0]);
	close(fds[1]);

	CHECK(set_nonblocking(-1) == -1, "set_nonblocking bad fd");
	CHECK(set_cloexec(-1) == -1, "set_cloexec bad fd");
}

static void test_htonf(void)
{
	static const float vals[] = {0.0f, 1.0f, -1.0f, 3.5f, 1e-10f, 1e10f};
	size_t i;

	for (i = 0; i < array_size(vals); i++) {
		float net = htonf(vals[i]);
		uint32_t raw, host_raw;

		CHECK(ntohf(net) == vals[i], "roundtrip of %g", vals[i]);

		memcpy(&raw, &net, sizeof(raw));
		memcpy(&host_raw, &vals[i], sizeof(host_raw));
		CHECK(raw == htonl(host_raw), "htonf byte order of %g",
		      vals[i]);
	}

	/* 1.0f is 0x3f800000: big-endian wire bytes 3f 80 00 00 */
	{
		float one = htonf(1.0f);
		uint8_t bytes[4];

		memcpy(bytes, &one, sizeof(bytes));
		CHECK(bytes[0] == 0x3f && bytes[1] == 0x80 && bytes[2] == 0
			      && bytes[3] == 0,
		      "1.0f wire bytes");
	}
}

static void test_sequence(void)
{
	uint64_t a, b, c;
	uint32_t s32;

	a = frr_sequence_next();
	b = frr_sequence_next();
	c = frr_sequence_next();
	CHECK(b > a, "sequence strictly increasing (%llu, %llu)",
	      (unsigned long long)a, (unsigned long long)b);
	CHECK(c > b, "sequence strictly increasing (%llu, %llu)",
	      (unsigned long long)b, (unsigned long long)c);

	s32 = frr_sequence32_next();
	CHECK(s32 > (uint32_t)c, "sequence32 advances past previous");
}

int main(void)
{
	test_readn_writen();
	test_writen_errors();
	test_fd_flags();
	test_htonf();
	test_sequence();

	if (fail) {
		printf("FAILED\n");
		return 1;
	}
	printf("OK\n");
	return 0;
}
