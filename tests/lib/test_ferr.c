// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * lib/ferr tests: per-thread error info, kinds, unique ids and log_ref table
 */

#include <zebra.h>

#include "ferr.h"

static int fail;

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAILED line %d: %s\n", __LINE__, #cond);       \
			fail = 1;                                              \
		}                                                              \
	} while (0)

static struct log_ref test_refs[] = {
	{
		.code = 0x06000101,
		.title = "first",
		.description = "first description",
		.suggestion = "first suggestion",
	},
	{
		.code = 0x06000102,
		.title = "second",
		.description = "second description",
		.suggestion = "second suggestion",
	},
	{
		.code = END_FERR,
	},
};

static void test_set_get_clear(void)
{
	const struct ferr *e;
	ferr_r rv;
	uint32_t id1, id2;

	CHECK(ferr_ok() == 0);
	CHECK(ferr_clear() == 0);
	CHECK(ferr_get_last(0) == NULL);

	rv = ferr_cfg_invalid("value %d out of range %s", 7, "x");
	CHECK(rv != 0);
	e = ferr_get_last(rv);
	CHECK(e != NULL);
	if (e) {
		CHECK(e->kind == FERR_CONFIG_INVALID);
		CHECK(strcmp(e->message, "value 7 out of range x") == 0);
		CHECK(strstr(e->file, "test_ferr") != NULL);
		CHECK(strcmp(e->func, "test_set_get_clear") == 0);
		CHECK(e->line > 0);
		CHECK(e->errno_val == 0);
		CHECK(e->pathname[0] == '\0');
		id1 = e->unique_id;

		/* same file + same format, different args: same id */
		rv = ferr_cfg_invalid("value %d out of range %s", 9, "y");
		e = ferr_get_last(rv);
		CHECK(e && e->unique_id == id1);
		CHECK(e && strcmp(e->message, "value 9 out of range y") == 0);

		/* different format: different id */
		rv = ferr_cfg_invalid("other format");
		e = ferr_get_last(rv);
		id2 = e ? e->unique_id : id1;
		CHECK(id2 != id1);
	}

	CHECK(ferr_clear() == 0);
	CHECK(ferr_get_last(rv) == NULL);
}

static void test_kinds(void)
{
	CHECK(ferr_get_last(ferr_code_bug("a")) != NULL);
	CHECK(ferr_get_last(0)->kind == FERR_CODE_BUG);
	ferr_cfg_reality("a");
	CHECK(ferr_get_last(0)->kind == FERR_CONFIG_REALITY);
	ferr_cfg_resource("a");
	CHECK(ferr_get_last(0)->kind == FERR_RESOURCE);
	ferr_system("a");
	CHECK(ferr_get_last(0)->kind == FERR_SYSTEM);
	ferr_library("a");
	CHECK(ferr_get_last(0)->kind == FERR_LIBRARY);
	ferr_clear();
}

static void test_ext(void)
{
	const struct ferr *e;

	errno = ENOENT;
	CHECK(ferr_system_errno("open failed") != 0);
	e = ferr_get_last(0);
	CHECK(e && e->kind == FERR_SYSTEM);
	CHECK(e && e->errno_val == ENOENT);
	CHECK(e && e->pathname[0] == '\0');

	errno = EACCES;
	CHECK(ferr_system_path_errno("/some/path", "cannot read %s", "cfg")
	      != 0);
	e = ferr_get_last(0);
	CHECK(e && e->errno_val == EACCES);
	CHECK(e && strcmp(e->pathname, "/some/path") == 0);
	CHECK(e && strcmp(e->message, "cannot read cfg") == 0);

	/* a later error without a path resets the pathname */
	ferr_cfg_invalid("plain");
	e = ferr_get_last(0);
	CHECK(e && e->pathname[0] == '\0' && e->errno_val == 0);
	ferr_clear();
}

static void test_truncation(void)
{
	const struct ferr *e;
	char big[1024];

	memset(big, 'a', sizeof(big) - 1);
	big[sizeof(big) - 1] = '\0';

	ferr_cfg_invalid("%s", big);
	e = ferr_get_last(0);
	CHECK(e != NULL);
	if (e) {
		CHECK(strlen(e->message) == sizeof(e->message) - 1);
		CHECK(e->message[0] == 'a');
	}
	ferr_clear();
}

static void test_log_ref(void)
{
	struct log_ref *r;

	log_ref_init();
	CHECK(log_ref_get(0x06000101) == NULL);

	log_ref_add(test_refs);

	r = log_ref_get(0x06000101);
	CHECK(r == &test_refs[0]);
	r = log_ref_get(0x06000102);
	CHECK(r && strcmp(r->title, "second") == 0);
	CHECK(r && strcmp(r->suggestion, "second suggestion") == 0);
	CHECK(log_ref_get(0x06000103) == NULL);
	/* terminator entry is never added */
	CHECK(log_ref_get(END_FERR) == NULL);

	log_ref_fini();
}

int main(int argc, char **argv)
{
	test_set_get_clear();
	test_kinds();
	test_ext();
	test_truncation();
	test_log_ref();

	printf("final tally: %s\n", fail ? "FAILED" : "ok");
	return fail;
}
