// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * sbuf fixed/dynamic init, push, indent, reset and growth tests
 */
#include <zebra.h>

#include "sbuf.h"

static int fail;

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL: line %d: %s\n", __LINE__, #cond);        \
			fail = 1;                                              \
		}                                                              \
	} while (0)

static void test_fixed(void)
{
	char storage[64];
	struct sbuf sb;

	sbuf_init(&sb, storage, sizeof(storage));
	CHECK(sb.fixed);
	CHECK(sb.size == sizeof(storage));
	CHECK(sb.pos == 0);
	CHECK(sbuf_buf(&sb) == storage);
	CHECK(strcmp(sbuf_buf(&sb), "") == 0);

	sbuf_push(&sb, 0, "hello");
	CHECK(strcmp(sbuf_buf(&sb), "hello") == 0);
	CHECK(sb.pos == 5);

	sbuf_push(&sb, 0, " %s %d", "world", 42);
	CHECK(strcmp(sbuf_buf(&sb), "hello world 42") == 0);

	sbuf_reset(&sb);
	CHECK(sb.pos == 0);
	CHECK(strcmp(sbuf_buf(&sb), "") == 0);

	/* freeing a fixed buffer must not free caller storage */
	sbuf_free(&sb);
	CHECK(sbuf_buf(&sb) == storage);
}

static void test_indent(void)
{
	char storage[64];
	struct sbuf sb;

	sbuf_init(&sb, storage, sizeof(storage));
	sbuf_push(&sb, 4, "a");
	sbuf_push(&sb, 0, "\n");
	sbuf_push(&sb, 2, "b");
	CHECK(strcmp(sbuf_buf(&sb), "    a\n  b") == 0);
	sbuf_free(&sb);
}

static void test_dynamic(void)
{
	struct sbuf sb;
	size_t expect = 0;
	int i;

	sbuf_init(&sb, NULL, 0);
	CHECK(!sb.fixed);
	CHECK(sb.buf != NULL);
	CHECK(sb.size == SBUF_DEFAULT_SIZE);
	CHECK(sb.pos == 0);
	CHECK(strcmp(sbuf_buf(&sb), "") == 0);

	/* push well beyond the default size to force reallocation */
	for (i = 0; i < 2000; i++) {
		sbuf_push(&sb, 1, "line%04d\n", i);
		expect += 1 + 9;
	}
	CHECK(sb.size > SBUF_DEFAULT_SIZE);
	CHECK(sb.pos == expect);
	CHECK(strlen(sbuf_buf(&sb)) == expect);
	CHECK(strncmp(sbuf_buf(&sb), " line0000\n line0001\n", 20) == 0);
	CHECK(strcmp(sbuf_buf(&sb) + expect - 10, " line1999\n") == 0);

	sbuf_reset(&sb);
	CHECK(sb.pos == 0);
	CHECK(strcmp(sbuf_buf(&sb), "") == 0);
	/* capacity is retained after reset */
	CHECK(sb.size > SBUF_DEFAULT_SIZE);

	sbuf_push(&sb, 0, "again");
	CHECK(strcmp(sbuf_buf(&sb), "again") == 0);

	sbuf_free(&sb);
}

int main(int argc, char **argv)
{
	test_fixed();
	test_indent();
	test_dynamic();

	if (fail)
		return 1;
	printf("sbuf tests passed\n");
	return 0;
}
