// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * lib/ptm_lib.c message build / parse unit tests
 */
#include <zebra.h>

#include <sys/socket.h>

#include "ptm_lib.h"

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

struct seen {
	int cmd, notify, response;
	char cmd_val[64];
	char extra_val[64];
	int missing_rc;
};

static int record(struct seen *s, void *ctxt)
{
	char val[64];

	if (ptm_lib_find_key_in_msg(ctxt, "cmd", val) == 0)
		snprintf(s->cmd_val, sizeof(s->cmd_val), "%s", val);
	if (ptm_lib_find_key_in_msg(ctxt, "extra", val) == 0)
		snprintf(s->extra_val, sizeof(s->extra_val), "%s", val);
	s->missing_rc = ptm_lib_find_key_in_msg(ctxt, "nokey", val);
	if (s->missing_rc != 0 && val[0] != '\0')
		s->missing_rc = 99;
	return 0;
}

static int cmd_cb(void *data, void *ctxt)
{
	struct seen *s = data;

	s->cmd++;
	return record(s, ctxt);
}

static int notify_cb(void *data, void *ctxt)
{
	struct seen *s = data;

	s->notify++;
	return record(s, ctxt);
}

static int response_cb(void *data, void *ctxt)
{
	struct seen *s = data;

	s->response++;
	return record(s, ctxt);
}

/* build a message of @type with cmd/extra keys into @buf, return length */
static int build(ptm_lib_handle_t *hdl, int type, char *buf, int bufsz)
{
	void *ctxt = NULL;
	int len = bufsz;

	CHECK(ptm_lib_init_msg(hdl, 7, type, NULL, &ctxt) == 0, "init_msg");
	CHECK(ctxt != NULL, "init_msg ctxt");
	CHECK(ptm_lib_append_msg(hdl, ctxt, "cmd", "start-bfd-sess") == 0,
	      "append cmd");
	CHECK(ptm_lib_append_msg(hdl, ctxt, "extra", "hello") == 0,
	      "append extra");
	CHECK(ptm_lib_complete_msg(hdl, ctxt, buf, &len) == 0, "complete_msg");
	return len;
}

static void feed(int fd, const char *buf, int len)
{
	CHECK(write(fd, buf, len) == len, "write to socketpair");
}

static void test_api_errors(void)
{
	ptm_lib_handle_t *hdl = ptm_lib_register("tester", NULL, NULL, NULL);
	char buf[PTMLIB_MSG_SZ];
	int len = sizeof(buf);

	CHECK(hdl != NULL, "register");
	CHECK(ptm_lib_append_msg(hdl, NULL, "k", "v") == -1,
	      "append with no ctxt");
	CHECK(ptm_lib_complete_msg(hdl, NULL, buf, &len) == -1,
	      "complete with no ctxt");
	CHECK(ptm_lib_cleanup_msg(hdl, NULL) == -1, "cleanup with no ctxt");
	ptm_lib_deregister(hdl);
	ptm_lib_deregister(NULL);
}

static void test_register(void)
{
	ptm_lib_handle_t *hdl =
		ptm_lib_register("tester", cmd_cb, notify_cb, response_cb);

	CHECK(hdl != NULL, "register");
	if (!hdl)
		return;
	CHECK(!strcmp(hdl->client_name, "tester"), "client name '%s'",
	      hdl->client_name);
	CHECK(hdl->cmd_cb == cmd_cb && hdl->notify_cb == notify_cb &&
		      hdl->response_cb == response_cb,
	      "callbacks stored");
	ptm_lib_deregister(hdl);

	/* over-long names are truncated, not overflowed */
	hdl = ptm_lib_register("0123456789abcdef0123456789abcdefXYZ", NULL,
			       NULL, NULL);
	CHECK(hdl && strlen(hdl->client_name) == PTMLIB_MAXNAMELEN - 1,
	      "truncated name length");
	ptm_lib_deregister(hdl);
}

static void test_header_format(void)
{
	ptm_lib_handle_t *hdl = ptm_lib_register("0123456789abcdefXYZ", NULL,
						 NULL, NULL);
	char buf[PTMLIB_MSG_SZ] = {0};
	int len = build(hdl, PTMLIB_MSG_TYPE_CMD, buf, sizeof(buf));

	CHECK(len > PTMLIB_MSG_HDR_LEN, "message length %d", len);
	CHECK(strstr(buf, "0123456789abcdef\n") != NULL,
	      "client name truncated to 16 chars: '%s'", buf);
	CHECK(strstr(buf, "XYZ") == NULL, "no client name overflow");
	CHECK(buf[PTMLIB_MSG_HDR_LEN - 1] == '\n', "header is %d bytes",
	      PTMLIB_MSG_HDR_LEN);
	CHECK(atoi(buf) == len - PTMLIB_MSG_HDR_LEN, "msglen field %d vs %d",
	      atoi(buf), len - PTMLIB_MSG_HDR_LEN);
	CHECK(strstr(buf, "cmd,extra\nstart-bfd-sess,hello\n") != NULL,
	      "body keys/values: '%s'", buf + PTMLIB_MSG_HDR_LEN);
	ptm_lib_deregister(hdl);
}

static void roundtrip(int type, int exp_cmd, int exp_notify, int exp_resp)
{
	ptm_lib_handle_t *hdl =
		ptm_lib_register("tester", cmd_cb, notify_cb, response_cb);
	struct seen s;
	char out[PTMLIB_MSG_SZ] = {0};
	char in[PTMLIB_MSG_SZ] = {0};
	int sv[2];
	int len, rc;

	memset(&s, 0, sizeof(s));
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv)) {
		CHECK(0, "socketpair");
		ptm_lib_deregister(hdl);
		return;
	}

	len = build(hdl, type, out, sizeof(out));
	feed(sv[0], out, len);

	rc = ptm_lib_process_msg(hdl, sv[1], in, sizeof(in), &s);
	CHECK(rc == len - PTMLIB_MSG_HDR_LEN, "process_msg rc %d, want %d", rc,
	      len - PTMLIB_MSG_HDR_LEN);
	CHECK(s.cmd == exp_cmd && s.notify == exp_notify &&
		      s.response == exp_resp,
	      "type %d dispatch cmd=%d notify=%d response=%d", type, s.cmd,
	      s.notify, s.response);
	CHECK(!strcmp(s.cmd_val, "start-bfd-sess"), "cmd value '%s'",
	      s.cmd_val);
	CHECK(!strcmp(s.extra_val, "hello"), "extra value '%s'", s.extra_val);
	CHECK(s.missing_rc == -1, "missing key lookup rc %d", s.missing_rc);

	close(sv[0]);
	close(sv[1]);
	ptm_lib_deregister(hdl);
}

static void test_roundtrip(void)
{
	roundtrip(PTMLIB_MSG_TYPE_CMD, 1, 0, 0);
	roundtrip(PTMLIB_MSG_TYPE_NOTIFICATION, 0, 1, 0);
	roundtrip(PTMLIB_MSG_TYPE_RESPONSE, 0, 0, 1);
}

static void test_no_callback(void)
{
	/* a type with no registered callback is consumed without crashing */
	ptm_lib_handle_t *hdl = ptm_lib_register("tester", NULL, NULL, NULL);
	char out[PTMLIB_MSG_SZ] = {0};
	char in[PTMLIB_MSG_SZ] = {0};
	int sv[2];
	int len, rc;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv)) {
		CHECK(0, "socketpair");
		ptm_lib_deregister(hdl);
		return;
	}
	len = build(hdl, PTMLIB_MSG_TYPE_CMD, out, sizeof(out));
	feed(sv[0], out, len);
	rc = ptm_lib_process_msg(hdl, sv[1], in, sizeof(in), NULL);
	CHECK(rc == len - PTMLIB_MSG_HDR_LEN, "rc %d", rc);
	close(sv[0]);
	close(sv[1]);
	ptm_lib_deregister(hdl);
}

static void test_msglen_too_big(void)
{
	ptm_lib_handle_t *hdl =
		ptm_lib_register("tester", cmd_cb, notify_cb, response_cb);
	struct seen s;
	char out[PTMLIB_MSG_SZ] = {0};
	char in[PTMLIB_MSG_SZ] = {0};
	int sv[2];
	int len, rc;

	memset(&s, 0, sizeof(s));
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv)) {
		CHECK(0, "socketpair");
		ptm_lib_deregister(hdl);
		return;
	}
	len = build(hdl, PTMLIB_MSG_TYPE_CMD, out, sizeof(out));
	feed(sv[0], out, len);
	/* caller buffer smaller than the advertised body length */
	rc = ptm_lib_process_msg(hdl, sv[1], in, 4, &s);
	CHECK(rc == -1, "oversized msglen rc %d", rc);
	CHECK(s.cmd == 0, "callback must not run");
	close(sv[0]);
	close(sv[1]);
	ptm_lib_deregister(hdl);
}

static void test_legacy_get_status(void)
{
	ptm_lib_handle_t *hdl =
		ptm_lib_register("tester", cmd_cb, notify_cb, response_cb);
	struct seen s;
	char in[PTMLIB_MSG_SZ] = {0};
	int sv[2];
	int rc;

	memset(&s, 0, sizeof(s));
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv)) {
		CHECK(0, "socketpair");
		ptm_lib_deregister(hdl);
		return;
	}

	/* bare legacy command is turned into a CMD message */
	feed(sv[0], PTMLIB_CMD_GET_STATUS, strlen(PTMLIB_CMD_GET_STATUS));
	rc = ptm_lib_process_msg(hdl, sv[1], in, sizeof(in), &s);
	CHECK(rc == (int)strlen(PTMLIB_CMD_GET_STATUS), "legacy rc %d", rc);
	CHECK(s.cmd == 1, "legacy dispatched as cmd (%d)", s.cmd);
	CHECK(!strcmp(s.cmd_val, PTMLIB_CMD_GET_STATUS), "legacy cmd '%s'",
	      s.cmd_val);

	/* any other legacy command is rejected */
	memset(in, 0, sizeof(in));
	memset(&s, 0, sizeof(s));
	feed(sv[0], "bogus", 5);
	rc = ptm_lib_process_msg(hdl, sv[1], in, sizeof(in), &s);
	CHECK(rc == -1, "unsupported legacy rc %d", rc);
	CHECK(s.cmd == 0, "unsupported legacy must not dispatch");

	close(sv[0]);
	close(sv[1]);
	ptm_lib_deregister(hdl);
}

static void test_closed_socket(void)
{
	ptm_lib_handle_t *hdl =
		ptm_lib_register("tester", cmd_cb, notify_cb, response_cb);
	char in[PTMLIB_MSG_SZ] = {0};
	int sv[2];
	int rc;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv)) {
		CHECK(0, "socketpair");
		ptm_lib_deregister(hdl);
		return;
	}
	close(sv[0]);
	rc = ptm_lib_process_msg(hdl, sv[1], in, sizeof(in), NULL);
	CHECK(rc <= 0, "closed peer rc %d", rc);
	close(sv[1]);
	ptm_lib_deregister(hdl);
}

int main(int argc, char **argv)
{
	test_api_errors();
	test_register();
	test_header_format();
	test_roundtrip();
	test_no_callback();
	test_msglen_too_big();
	test_legacy_get_status();
	test_closed_socket();

	if (!fail)
		printf("ptm_lib tests passed\n");
	return fail;
}
