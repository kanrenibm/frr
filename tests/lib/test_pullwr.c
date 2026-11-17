// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * lib/pullwr.c tests: data written through fill() arrives intact and in
 * order across a full socket buffer (EAGAIN path), stats are consistent, and
 * a write error on a closed peer reaches the err() callback.
 */
#include <zebra.h>

#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>

#include "frrevent.h"
#include "pullwr.h"

#define CHUNK 1000
#define NCHUNKS 700 /* 700 kB: larger than a default socketpair buffer */
#define TOTAL ((size_t)CHUNK * NCHUNKS)

static int fail;

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL: line %d: %s\n", __LINE__, #cond);        \
			fail = 1;                                              \
		}                                                              \
	} while (0)

struct ctx {
	struct event_loop *master;
	struct pullwr *pw;
	int rfd;
	size_t produced;
	size_t received;
	bool mismatch;
	bool done;
	int err_calls;
	bool err_eof;
};

static uint8_t pattern(size_t off)
{
	return (uint8_t)((off * 31 + (off >> 8)) & 0xff);
}

static void fill_cb(struct ctx *c, struct pullwr *pw)
{
	uint8_t buf[CHUNK];
	size_t i;

	if (c->produced >= TOTAL)
		return; /* pushing nothing makes pullwr go idle */

	for (i = 0; i < CHUNK; i++)
		buf[i] = pattern(c->produced + i);
	c->produced += CHUNK;
	pullwr_write(pw, buf, CHUNK);
}

static void err_cb(struct ctx *c, struct pullwr *pw, bool eof)
{
	c->err_calls++;
	c->err_eof = eof;
}

static void reader(struct event *e)
{
	struct ctx *c = EVENT_ARG(e);
	uint8_t buf[4096];
	ssize_t n;
	ssize_t i;

	while ((n = read(c->rfd, buf, sizeof(buf))) > 0) {
		for (i = 0; i < n; i++)
			if (buf[i] != pattern(c->received + i))
				c->mismatch = true;
		c->received += n;
	}
	if (c->received >= TOTAL) {
		c->done = true;
		return;
	}
	event_add_read(c->master, reader, c, c->rfd, NULL);
}

static void setnb(int fd)
{
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

static void test_transfer(void)
{
	struct ctx c = { 0 };
	struct event ev;
	int sv[2];
	uint64_t written;
	size_t pending, kpending;

	CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	setnb(sv[0]);
	setnb(sv[1]);

	c.master = event_master_create("pullwr-test");
	c.rfd = sv[1];
	c.pw = pullwr_new(c.master, sv[0], &c, fill_cb, err_cb);
	pullwr_cfg(c.pw, 2500, 4096);

	event_add_read(c.master, reader, &c, c.rfd, NULL);
	pullwr_bump(c.pw);

	while (!c.done && event_fetch(c.master, &ev))
		event_call(&ev);

	CHECK(c.done);
	CHECK(c.produced == TOTAL);
	CHECK(c.received == TOTAL);
	CHECK(!c.mismatch);
	CHECK(c.err_calls == 0);

	pullwr_stats(c.pw, &written, &pending, &kpending);
	CHECK(written == TOTAL);
	CHECK(pending == 0);

	pullwr_del(c.pw);
	event_master_free(c.master);
	close(sv[0]);
	close(sv[1]);
}

static void test_error(void)
{
	struct ctx c = { 0 };
	struct event ev;
	int sv[2];

	CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	setnb(sv[0]);
	close(sv[1]); /* peer gone: writev() fails with EPIPE */

	c.master = event_master_create("pullwr-err");
	c.pw = pullwr_new(c.master, sv[0], &c, fill_cb, err_cb);
	pullwr_bump(c.pw);

	while (!c.err_calls && event_fetch(c.master, &ev))
		event_call(&ev);

	CHECK(c.err_calls == 1);
	CHECK(!c.err_eof);

	pullwr_del(c.pw);
	event_master_free(c.master);
	close(sv[0]);
}

int main(void)
{
	signal(SIGPIPE, SIG_IGN);

	test_transfer();
	test_error();

	if (!fail)
		printf("pullwr tests passed\n");
	return fail;
}
