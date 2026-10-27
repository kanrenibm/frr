// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * qobj register/lookup/typed-lookup/unregister tests
 */
#include <zebra.h>

#include "qobj.h"

static int fail;

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL: line %d: %s\n", __LINE__, #cond);        \
			fail = 1;                                              \
		}                                                              \
	} while (0)

struct obj_a {
	int pad;
	QOBJ_FIELDS;
	int value;
};
DECLARE_QOBJ_TYPE(obj_a);
DEFINE_QOBJ_TYPE(obj_a);

struct obj_b {
	QOBJ_FIELDS;
	int value;
};
DECLARE_QOBJ_TYPE(obj_b);
DEFINE_QOBJ_TYPE(obj_b);

static void test_reg_lookup(void)
{
	struct obj_a a = {.value = 1};
	struct obj_b b = {.value = 2};

	QOBJ_REG((&a), obj_a);
	QOBJ_REG((&b), obj_b);

	/* ids are non-zero and unique */
	CHECK(QOBJ_ID(&a) != 0);
	CHECK(QOBJ_ID(&b) != 0);
	CHECK(QOBJ_ID(&a) != QOBJ_ID(&b));

	/* untyped lookup returns the embedded node */
	CHECK(qobj_get(QOBJ_ID(&a)) == &a.qobj_node);
	CHECK(qobj_get(QOBJ_ID(&b)) == &b.qobj_node);

	/* typed lookup returns the containing struct, even at a nonzero
	 * member offset
	 */
	CHECK(QOBJ_GET_TYPESAFE(QOBJ_ID(&a), obj_a) == &a);
	CHECK(QOBJ_GET_TYPESAFE(QOBJ_ID(&b), obj_b) == &b);

	/* wrong type yields NULL */
	CHECK(QOBJ_GET_TYPESAFE(QOBJ_ID(&a), obj_b) == NULL);
	CHECK(QOBJ_GET_TYPESAFE(QOBJ_ID(&b), obj_a) == NULL);

	QOBJ_UNREG((&a));
	QOBJ_UNREG((&b));
}

static void test_unreg_and_missing(void)
{
	struct obj_a a = {.value = 1};
	uint64_t id;

	QOBJ_REG((&a), obj_a);
	id = QOBJ_ID(&a);
	CHECK(qobj_get(id) == &a.qobj_node);

	QOBJ_UNREG((&a));
	CHECK(qobj_get(id) == NULL);
	CHECK(QOBJ_GET_TYPESAFE(id, obj_a) == NULL);

	/* id 0 is never allocated */
	CHECK(qobj_get(0) == NULL);
	CHECK(QOBJ_GET_TYPESAFE(0, obj_a) == NULL);
}

static void test_id_0safe(void)
{
	struct obj_a a = {.value = 1};
	struct obj_a *none = NULL;

	QOBJ_REG((&a), obj_a);
	CHECK(QOBJ_ID_0SAFE(&a) == QOBJ_ID(&a));
	CHECK(QOBJ_ID_0SAFE(none) == 0);
	QOBJ_UNREG((&a));
}

static void test_many(void)
{
	enum { N = 64 };
	struct obj_b objs[N];
	int i, j;

	for (i = 0; i < N; i++) {
		objs[i].value = i;
		QOBJ_REG((&objs[i]), obj_b);
	}

	for (i = 0; i < N; i++) {
		CHECK(QOBJ_GET_TYPESAFE(QOBJ_ID(&objs[i]), obj_b) == &objs[i]);
		for (j = i + 1; j < N; j++)
			CHECK(QOBJ_ID(&objs[i]) != QOBJ_ID(&objs[j]));
	}

	for (i = 0; i < N; i++)
		QOBJ_UNREG((&objs[i]));
	for (i = 0; i < N; i++)
		CHECK(qobj_get(QOBJ_ID(&objs[i])) == NULL);
}

int main(int argc, char **argv)
{
	qobj_init();

	test_reg_lookup();
	test_unreg_and_missing();
	test_id_0safe();
	test_many();

	qobj_finish();

	if (!fail)
		printf("OK\n");
	return fail;
}
