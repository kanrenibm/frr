// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * lib/log_filter.c filter list tests (zlog_filter_add/del/dump/clear)
 */
#include <zebra.h>

#include "log.h"

static int fail;

static void check_int(const char *what, int got, int expect)
{
	if (got != expect) {
		printf("FAIL: %s: got %d, expected %d\n", what, got, expect);
		fail = 1;
	}
}

static void check_dump(const char *expect)
{
	char buf[ZLOG_FILTERS_MAX * (ZLOG_FILTER_LENGTH_MAX + 3) + 1];
	int len;

	buf[0] = '\0';
	len = zlog_filter_dump(buf, sizeof(buf));

	check_int("dump len", len, (int)strlen(expect));
	if (len >= 0 && strcmp(buf, expect)) {
		printf("FAIL: dump \"%s\", expected \"%s\"\n", buf, expect);
		fail = 1;
	}
}

int main(int argc, char **argv)
{
	char buf[16];
	char name[ZLOG_FILTER_LENGTH_MAX + 1];
	int i;

	check_dump("");

	check_int("add a", zlog_filter_add("a"), 0);
	check_int("add b", zlog_filter_add("b"), 0);
	check_int("add c", zlog_filter_add("c"), 0);
	check_int("add dup", zlog_filter_add("b"), -1);
	check_int("add empty", zlog_filter_add(""), -1);
	check_dump(" a\n b\n c\n");

	/* delete middle, then first, then last: array must stay compact */
	check_int("del missing", zlog_filter_del("zz"), -1);
	check_int("del b", zlog_filter_del("b"), 0);
	check_dump(" a\n c\n");
	check_int("del b again", zlog_filter_del("b"), -1);
	check_int("del a", zlog_filter_del("a"), 0);
	check_dump(" c\n");
	check_int("del c", zlog_filter_del("c"), 0);
	check_dump("");

	/* dump into a buffer that is too small */
	zlog_filter_add("abcdef");
	check_int("dump small", zlog_filter_dump(buf, 4), -1);
	zlog_filter_clear();
	check_dump("");

	/* overlong filters are truncated to ZLOG_FILTER_LENGTH_MAX */
	memset(name, 'x', sizeof(name) - 1);
	name[sizeof(name) - 1] = '\0';
	check_int("add long", zlog_filter_add(name), 0);
	zlog_filter_clear();

	/* fill to the limit */
	for (i = 0; i < ZLOG_FILTERS_MAX; i++) {
		snprintf(name, sizeof(name), "f%d", i);
		check_int("fill", zlog_filter_add(name), 0);
	}
	check_int("add over max", zlog_filter_add("extra"), 1);
	check_int("del last", zlog_filter_del("f99"), 0);
	check_int("add after del", zlog_filter_add("extra"), 0);

	zlog_filter_clear();
	check_dump("");
	check_int("add after clear", zlog_filter_add("f0"), 0);
	zlog_filter_clear();

	return fail;
}
