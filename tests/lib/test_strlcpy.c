// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * strlcpy / strlcat unit tests
 */
#include <zebra.h>

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

static void test_strlcpy(void)
{
	char buf[8];
	size_t r;

	/* fits, with room to spare */
	memset(buf, 'x', sizeof(buf));
	r = strlcpy(buf, "abc", sizeof(buf));
	CHECK(r == 3, "fit: ret %zu", r);
	CHECK(!strcmp(buf, "abc"), "fit: buf '%s'", buf);

	/* exact fit: len + NUL == size */
	memset(buf, 'x', sizeof(buf));
	r = strlcpy(buf, "1234567", sizeof(buf));
	CHECK(r == 7, "exact: ret %zu", r);
	CHECK(!strcmp(buf, "1234567"), "exact: buf '%s'", buf);

	/* truncation: returns full source length, always NUL terminated */
	memset(buf, 'x', sizeof(buf));
	r = strlcpy(buf, "0123456789", sizeof(buf));
	CHECK(r == 10, "trunc: ret %zu", r);
	CHECK(!strcmp(buf, "0123456"), "trunc: buf '%s'", buf);

	/* size 1: only the NUL terminator is written */
	memset(buf, 'x', sizeof(buf));
	r = strlcpy(buf, "abc", 1);
	CHECK(r == 3, "size1: ret %zu", r);
	CHECK(buf[0] == '\0' && buf[1] == 'x', "size1: buf modified wrongly");

	/* size 0: nothing is written, length still returned */
	memset(buf, 'x', sizeof(buf));
	r = strlcpy(buf, "abc", 0);
	CHECK(r == 3, "size0: ret %zu", r);
	CHECK(buf[0] == 'x', "size0: buffer was written");

	/* empty source */
	memset(buf, 'x', sizeof(buf));
	r = strlcpy(buf, "", sizeof(buf));
	CHECK(r == 0 && buf[0] == '\0', "empty: ret %zu", r);
}

static void test_strlcat(void)
{
	char buf[8];
	size_t r;

	/* simple append */
	strlcpy(buf, "ab", sizeof(buf));
	r = strlcat(buf, "cd", sizeof(buf));
	CHECK(r == 4, "append: ret %zu", r);
	CHECK(!strcmp(buf, "abcd"), "append: buf '%s'", buf);

	/* exact fit */
	strlcpy(buf, "abc", sizeof(buf));
	r = strlcat(buf, "defg", sizeof(buf));
	CHECK(r == 7, "exact: ret %zu", r);
	CHECK(!strcmp(buf, "abcdefg"), "exact: buf '%s'", buf);

	/* truncation: returns length it would have had */
	strlcpy(buf, "abcde", sizeof(buf));
	r = strlcat(buf, "fghij", sizeof(buf));
	CHECK(r == 10, "trunc: ret %zu", r);
	CHECK(!strcmp(buf, "abcdefg"), "trunc: buf '%s'", buf);

	/* destination already full / unterminated within size */
	memset(buf, 'x', sizeof(buf));
	r = strlcat(buf, "abc", sizeof(buf));
	CHECK(r == sizeof(buf) + 3, "full: ret %zu", r);
	CHECK(buf[0] == 'x' && buf[7] == 'x', "full: buffer was modified");

	/* size 0: nothing written, returns source length */
	strlcpy(buf, "ab", sizeof(buf));
	r = strlcat(buf, "cd", 0);
	CHECK(r == 2, "size0: ret %zu", r);
	CHECK(!strcmp(buf, "ab"), "size0: buf '%s'", buf);

	/* empty source leaves destination unchanged */
	strlcpy(buf, "ab", sizeof(buf));
	r = strlcat(buf, "", sizeof(buf));
	CHECK(r == 2 && !strcmp(buf, "ab"), "empty src: ret %zu", r);
}

int main(int argc, char **argv)
{
	test_strlcpy();
	test_strlcat();

	printf("%s\n", fail ? "FAILED" : "all strlcpy/strlcat tests passed");
	return fail;
}
