// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * lib/flex_algo database, definition comparison and helper tests
 */
#include <zebra.h>

#include "flex_algo.h"

static int fail;

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL: line %d: %s\n", __LINE__, #cond);        \
			fail = 1;                                              \
		}                                                              \
	} while (0)

static int allocs;
static int releases;
static int last_arg;

static void *test_allocator(void *arg)
{
	allocs++;
	last_arg = *(int *)arg;
	return arg;
}

static void test_releaser(void *data)
{
	CHECK(data != NULL);
	releases++;
}

static void test_id_valid(void)
{
	CHECK(!flex_algo_id_valid(0));
	CHECK(!flex_algo_id_valid(1));
	CHECK(!flex_algo_id_valid(SR_ALGORITHM_FLEX_MIN - 1));
	CHECK(flex_algo_id_valid(SR_ALGORITHM_FLEX_MIN));
	CHECK(flex_algo_id_valid(200));
	CHECK(flex_algo_id_valid(SR_ALGORITHM_FLEX_MAX));
	CHECK(!flex_algo_id_valid(SR_ALGORITHM_FLEX_MAX + 1));
	CHECK(!flex_algo_id_valid(UINT16_MAX));
}

static void test_metric_type_print(void)
{
	char buf[16];

	CHECK(flex_algo_metric_type_print(buf, sizeof(buf), MT_IGP) == buf);
	CHECK(!strcmp(buf, "igp"));
	flex_algo_metric_type_print(buf, sizeof(buf), MT_MIN_UNI_LINK_DELAY);
	CHECK(!strcmp(buf, "delay"));
	flex_algo_metric_type_print(buf, sizeof(buf), MT_TE_DEFAULT);
	CHECK(!strcmp(buf, "te"));

	/* snprintf truncation must stay NUL terminated */
	flex_algo_metric_type_print(buf, 3, MT_MIN_UNI_LINK_DELAY);
	CHECK(!strcmp(buf, "de"));
}

static void test_database(void)
{
	struct flex_algos *db;
	struct flex_algo *fa128, *fa129;
	int arg1 = 11, arg2 = 22;

	allocs = releases = 0;
	db = flex_algos_alloc(test_allocator, test_releaser);
	CHECK(db != NULL);
	CHECK(flex_algo_lookup(db, 128) == NULL);

	fa128 = flex_algo_alloc(db, 128, &arg1);
	CHECK(fa128 != NULL);
	CHECK(fa128->algorithm == 128);
	CHECK(fa128->data == &arg1);
	CHECK(allocs == 1 && last_arg == 11);
	CHECK(admin_group_zero(&fa128->admin_group_exclude_any));
	CHECK(admin_group_zero(&fa128->admin_group_include_any));
	CHECK(admin_group_zero(&fa128->admin_group_include_all));

	fa129 = flex_algo_alloc(db, 129, &arg2);
	CHECK(fa129 != NULL && fa129 != fa128);
	CHECK(allocs == 2 && last_arg == 22);

	CHECK(flex_algo_lookup(db, 128) == fa128);
	CHECK(flex_algo_lookup(db, 129) == fa129);
	CHECK(flex_algo_lookup(db, 130) == NULL);

	/* state accessors */
	CHECK(!flex_algo_get_state(db, 128));
	flex_algo_set_state(db, 128, true);
	CHECK(flex_algo_get_state(db, 128));
	CHECK(!flex_algo_get_state(db, 129));
	/* unknown algorithm: get is false, set is a no-op */
	CHECK(!flex_algo_get_state(db, 200));
	flex_algo_set_state(db, 200, true);
	CHECK(!flex_algo_get_state(db, 200));
	flex_algo_set_state(db, 128, false);
	CHECK(!flex_algo_get_state(db, 128));

	/* freeing one entry releases it and removes it from the lookup */
	flex_algo_free(db, fa128);
	CHECK(releases == 1);
	CHECK(flex_algo_lookup(db, 128) == NULL);
	CHECK(flex_algo_lookup(db, 129) == fa129);

	/* freeing the database releases whatever is left */
	flex_algos_free(db);
	CHECK(releases == 2);
}

static void test_database_no_callbacks(void)
{
	struct flex_algos *db = flex_algos_alloc(NULL, NULL);
	struct flex_algo *fa;
	int arg = 5;

	fa = flex_algo_alloc(db, 130, &arg);
	CHECK(fa != NULL);
	CHECK(fa->data == NULL);
	CHECK(flex_algo_lookup(db, 130) == fa);
	flex_algos_free(db);
}

static void test_definition_cmp(void)
{
	struct flex_algos *db = flex_algos_alloc(NULL, NULL);
	struct flex_algo *a = flex_algo_alloc(db, 128, NULL);
	struct flex_algo *b = flex_algo_alloc(db, 128, NULL);
	struct flex_algo *c = flex_algo_alloc(db, 129, NULL);

	CHECK(flex_algo_definition_cmp(a, a));
	CHECK(flex_algo_definition_cmp(a, b));
	/* different algorithm number */
	CHECK(!flex_algo_definition_cmp(a, c));

	b->calc_type = 1;
	CHECK(!flex_algo_definition_cmp(a, b));
	b->calc_type = a->calc_type;
	CHECK(flex_algo_definition_cmp(a, b));

	b->metric_type = MT_TE_DEFAULT;
	CHECK(!flex_algo_definition_cmp(a, b));
	b->metric_type = a->metric_type;

	b->exclude_srlg = true;
	CHECK(!flex_algo_definition_cmp(a, b));
	b->exclude_srlg = a->exclude_srlg;

	b->flags = FAD_FLAG_M;
	CHECK(!flex_algo_definition_cmp(a, b));
	b->flags = a->flags;

	b->unsupported_subtlv = true;
	CHECK(!flex_algo_definition_cmp(a, b));
	b->unsupported_subtlv = a->unsupported_subtlv;

	/* each of the three admin-group sets participates in the comparison */
	admin_group_set(&b->admin_group_exclude_any, 3);
	CHECK(!flex_algo_definition_cmp(a, b));
	admin_group_set(&a->admin_group_exclude_any, 3);
	CHECK(flex_algo_definition_cmp(a, b));

	admin_group_set(&b->admin_group_include_all, 40);
	CHECK(!flex_algo_definition_cmp(a, b));
	admin_group_set(&a->admin_group_include_all, 40);
	CHECK(flex_algo_definition_cmp(a, b));

	admin_group_set(&b->admin_group_include_any, 7);
	CHECK(!flex_algo_definition_cmp(a, b));
	admin_group_set(&a->admin_group_include_any, 7);
	CHECK(flex_algo_definition_cmp(a, b));

	/* local-only fields are not part of the definition */
	b->state = true;
	b->advertise_definition = true;
	b->priority = 1;
	b->dataplanes = FLEX_ALGO_SRV6;
	CHECK(flex_algo_definition_cmp(a, b));

	flex_algos_free(db);
}

int main(int argc, char **argv)
{
	test_id_valid();
	test_metric_type_print();
	test_database();
	test_database_no_callbacks();
	test_definition_cmp();

	if (fail)
		return 1;
	printf("flex_algo tests passed\n");
	return 0;
}
