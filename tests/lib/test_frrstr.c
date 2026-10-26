// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * frrstr split/join/replace/prefix/suffix/hex/quote-aware scan tests
 */
#include <zebra.h>

#include "frrstr.h"
#include "memory.h"

static int fail;

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL: line %d: %s\n", __LINE__, #cond);        \
			fail = 1;                                              \
		}                                                              \
	} while (0)

static void test_split_join(void)
{
	char **parts = NULL;
	int argc = 0;
	char *joined;

	frrstr_split("a,b,,c", ",", &parts, &argc);
	CHECK(argc == 4);
	CHECK(!strcmp(parts[0], "a"));
	CHECK(!strcmp(parts[1], "b"));
	CHECK(!strcmp(parts[2], ""));
	CHECK(!strcmp(parts[3], "c"));

	joined = frrstr_join((const char **)parts, argc, "-");
	CHECK(joined && !strcmp(joined, "a-b--c"));
	XFREE(MTYPE_TMP, joined);

	joined = frrstr_join((const char **)parts, argc, NULL);
	CHECK(joined && !strcmp(joined, "abc"));
	XFREE(MTYPE_TMP, joined);

	for (int i = 0; i < argc; i++)
		XFREE(MTYPE_TMP, parts[i]);
	XFREE(MTYPE_TMP, parts);

	CHECK(frrstr_join(NULL, 0, ",") == NULL);
	CHECK(frrstr_split_vec(NULL, ",") == NULL);
}

static void test_split_vec(void)
{
	vector v = frrstr_split_vec("one two three", " ");
	char *joined;

	CHECK(v != NULL);
	CHECK(vector_active(v) == 3);
	CHECK(!strcmp(vector_slot(v, 0), "one"));
	CHECK(!strcmp(vector_slot(v, 2), "three"));

	joined = frrstr_join_vec(v, "/");
	CHECK(joined && !strcmp(joined, "one/two/three"));
	XFREE(MTYPE_TMP, joined);

	frrstr_strvec_free(v);
	frrstr_strvec_free(NULL);
}

static void test_replace(void)
{
	char *s;

	s = frrstr_replace("aXbXc", "X", "--");
	CHECK(!strcmp(s, "a--b--c"));
	XFREE(MTYPE_TMP, s);

	s = frrstr_replace("a--b--c", "--", "X");
	CHECK(!strcmp(s, "aXbXc"));
	XFREE(MTYPE_TMP, s);

	s = frrstr_replace("abc", "abc", "");
	CHECK(!strcmp(s, ""));
	XFREE(MTYPE_TMP, s);

	s = frrstr_replace("abc", "zzz", "y");
	CHECK(!strcmp(s, "abc"));
	XFREE(MTYPE_TMP, s);
}

static void test_prefix_suffix(void)
{
	CHECK(frrstr_startswith("foobar", "foo"));
	CHECK(frrstr_startswith("foo", "foo"));
	CHECK(frrstr_startswith("foo", ""));
	CHECK(!frrstr_startswith("fo", "foo"));
	CHECK(!frrstr_startswith("foobar", "bar"));
	CHECK(!frrstr_startswith(NULL, "foo"));
	CHECK(!frrstr_startswith("foo", NULL));

	CHECK(frrstr_endswith("foobar", "bar"));
	CHECK(frrstr_endswith("bar", "bar"));
	CHECK(frrstr_endswith("bar", ""));
	CHECK(!frrstr_endswith("ar", "bar"));
	CHECK(!frrstr_endswith("foobar", "foo"));
	CHECK(!frrstr_endswith(NULL, "bar"));
	CHECK(!frrstr_endswith("bar", NULL));
}

static void test_digit_hex(void)
{
	char buf[16];
	const uint8_t data[] = {0x00, 0x0f, 0xab, 0xff};

	CHECK(all_digit("0123456789"));
	CHECK(all_digit(""));
	CHECK(!all_digit("12a"));
	CHECK(!all_digit("-1"));

	CHECK(frrstr_hex(buf, sizeof(buf), data, sizeof(data)) == buf);
	CHECK(!strcmp(buf, "000fabff"));

	/* output is truncated to the buffer, never overrun */
	memset(buf, 'x', sizeof(buf));
	frrstr_hex(buf, 5, data, sizeof(data));
	CHECK(!strcmp(buf, "000f"));

	/* zero-size buffer is left untouched */
	buf[0] = 'q';
	CHECK(frrstr_hex(buf, 0, data, sizeof(data)) == buf);
	CHECK(buf[0] == 'q');
}

static void test_quote_scan(void)
{
	const char *s;

	s = "ab,cd";
	CHECK(frrstr_skip_over_char(s, ',') == s + 3);
	CHECK(frrstr_skip_over_char(s, ';') == NULL);

	/* separators inside quotes or after a backslash are ignored */
	s = "\"a,b\",c";
	CHECK(frrstr_skip_over_char(s, ',') == s + 6);
	s = "'a,b',c";
	CHECK(frrstr_skip_over_char(s, ',') == s + 6);
	s = "a\\,b,c";
	CHECK(frrstr_skip_over_char(s, ',') == s + 5);
	/* trailing lone backslash */
	CHECK(frrstr_skip_over_char("ab\\", ',') == NULL);

	/* fast path: no quoting characters, plain strrchr */
	s = "a,b,c";
	CHECK(frrstr_back_to_char(s, ',') == s + 3);
	CHECK(frrstr_back_to_char("", ',') == NULL);
	CHECK(frrstr_back_to_char("abc", ',') == NULL);

	/* slow path: last unquoted separator wins */
	s = "a,\"b,c\"";
	CHECK(frrstr_back_to_char(s, ',') == s + 1);
	s = "a\\,b";
	CHECK(frrstr_back_to_char(s, ',') == NULL);
}

int main(int argc, char **argv)
{
	test_split_join();
	test_split_vec();
	test_replace();
	test_prefix_suffix();
	test_digit_hex();
	test_quote_scan();

	if (!fail)
		printf("OK\n");
	return fail;
}
