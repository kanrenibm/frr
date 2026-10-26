// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * vector set/unset/remove/compact/lookup/array conversion tests
 */
#include <zebra.h>

#include "vector.h"
#include "memory.h"

static int fail;

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL: line %d: %s\n", __LINE__, #cond);        \
			fail = 1;                                              \
		}                                                              \
	} while (0)

static int items[16];

static void test_init_set(void)
{
	vector v = vector_init(0);
	unsigned int i;

	/* a zero size request still allocates one slot */
	CHECK(v->alloced == 1);
	CHECK(vector_active(v) == 0);
	CHECK(vector_count(v) == 0);
	CHECK(vector_empty_slot(v) == 0);
	CHECK(vector_lookup(v, 0) == NULL);

	for (i = 0; i < 10; i++)
		CHECK(vector_set(v, &items[i]) == (int)i);
	CHECK(vector_active(v) == 10);
	CHECK(vector_count(v) == 10);
	CHECK(v->alloced >= 10);
	for (i = 0; i < 10; i++)
		CHECK(vector_slot(v, i) == &items[i]);

	vector_free(v);
}

static void test_set_index(void)
{
	vector v = vector_init(1);

	/* sparse set grows active to index + 1 but counts one item */
	CHECK(vector_set_index(v, 5, &items[5]) == 5);
	CHECK(vector_active(v) == 6);
	CHECK(vector_count(v) == 1);
	CHECK(vector_lookup(v, 2) == NULL);
	CHECK(vector_lookup(v, 5) == &items[5]);

	/* vector_set fills the smallest empty slot */
	CHECK(vector_empty_slot(v) == 0);
	CHECK(vector_set(v, &items[0]) == 0);
	CHECK(vector_set(v, &items[1]) == 1);
	CHECK(vector_count(v) == 3);
	CHECK(vector_active(v) == 6);

	/* replacing a value keeps count, replacing with NULL drops it */
	vector_set_index(v, 5, &items[6]);
	CHECK(vector_count(v) == 3);
	CHECK(vector_lookup(v, 5) == &items[6]);
	vector_set_index(v, 5, NULL);
	CHECK(vector_count(v) == 2);
	CHECK(vector_active(v) == 6);

	vector_free(v);
}

static void test_lookup(void)
{
	vector v = vector_init(1);

	vector_set(v, &items[0]);

	/* lookup is bounded by active, lookup_ensure only grows storage */
	CHECK(vector_lookup(v, 1) == NULL);
	CHECK(vector_lookup(v, 1000) == NULL);
	CHECK(vector_lookup_ensure(v, 100) == NULL);
	CHECK(v->alloced > 100);
	CHECK(vector_active(v) == 1);
	CHECK(vector_count(v) == 1);

	vector_free(v);
}

static void test_unset(void)
{
	vector v = vector_init(1);

	vector_set_index(v, 0, &items[0]);
	vector_set_index(v, 3, &items[3]);
	CHECK(vector_active(v) == 4);
	CHECK(vector_count(v) == 2);

	/* out of range is a no-op */
	vector_unset(v, 10);
	CHECK(vector_active(v) == 4);
	CHECK(vector_count(v) == 2);

	/* unsetting the last item also trims trailing empty slots */
	vector_unset(v, 3);
	CHECK(vector_active(v) == 1);
	CHECK(vector_count(v) == 1);

	/* unsetting a middle item leaves a hole */
	vector_set_index(v, 1, &items[1]);
	vector_set_index(v, 2, &items[2]);
	vector_unset(v, 1);
	CHECK(vector_active(v) == 3);
	CHECK(vector_count(v) == 2);
	CHECK(vector_lookup(v, 1) == NULL);
	CHECK(vector_empty_slot(v) == 1);

	/* trimming stops at the previous non-NULL slot */
	vector_unset(v, 2);
	CHECK(vector_active(v) == 1);
	CHECK(vector_count(v) == 1);

	vector_unset(v, 0);
	CHECK(vector_active(v) == 0);
	CHECK(vector_count(v) == 0);

	vector_free(v);
}

static void test_unset_value(void)
{
	vector v = vector_init(1);

	vector_set(v, &items[0]);
	vector_set(v, &items[1]);
	vector_set(v, &items[2]);

	/* unknown value changes nothing */
	vector_unset_value(v, &items[9]);
	CHECK(vector_active(v) == 3);
	CHECK(vector_count(v) == 3);

	vector_unset_value(v, &items[1]);
	CHECK(vector_lookup(v, 1) == NULL);
	CHECK(vector_active(v) == 3);
	CHECK(vector_count(v) == 2);

	/* removing the tail trims through the hole */
	vector_unset_value(v, &items[2]);
	CHECK(vector_active(v) == 1);
	CHECK(vector_count(v) == 1);

	vector_free(v);
}

static void test_remove_compact(void)
{
	vector v = vector_init(1);

	vector_set(v, &items[0]);
	vector_set(v, &items[1]);
	vector_set(v, &items[2]);

	/* remove shifts later items down */
	vector_remove(v, 0);
	CHECK(vector_active(v) == 2);
	CHECK(vector_count(v) == 2);
	CHECK(vector_slot(v, 0) == &items[1]);
	CHECK(vector_slot(v, 1) == &items[2]);

	/* out of range is a no-op */
	vector_remove(v, 5);
	CHECK(vector_active(v) == 2);
	CHECK(vector_count(v) == 2);

	vector_free(v);

	/* compact squeezes out every hole, keeping order */
	v = vector_init(1);
	vector_set_index(v, 0, &items[0]);
	vector_set_index(v, 2, &items[2]);
	vector_set_index(v, 3, &items[3]);
	vector_set_index(v, 6, &items[6]);
	CHECK(vector_active(v) == 7);
	CHECK(vector_count(v) == 4);

	vector_compact(v);
	CHECK(vector_active(v) == 4);
	CHECK(vector_count(v) == 4);
	CHECK(vector_slot(v, 0) == &items[0]);
	CHECK(vector_slot(v, 1) == &items[2]);
	CHECK(vector_slot(v, 2) == &items[3]);
	CHECK(vector_slot(v, 3) == &items[6]);

	vector_free(v);
}

static void test_array_conversion(void)
{
	void *src[4] = {&items[0], &items[1], &items[2], &items[3]};
	void **arr = NULL;
	int argc = -1;
	vector v;

	v = array_to_vector(src, 4);
	CHECK(vector_active(v) == 4);
	CHECK(vector_count(v) == 4);
	CHECK(vector_slot(v, 3) == &items[3]);

	vector_to_array(v, &arr, &argc);
	CHECK(argc == 4);
	CHECK(arr != NULL);
	CHECK(memcmp(arr, src, sizeof(src)) == 0);
	XFREE(MTYPE_TMP, arr);
	vector_free(v);

	/* empty round trip */
	v = array_to_vector(src, 0);
	CHECK(vector_active(v) == 0);
	vector_to_array(v, &arr, &argc);
	CHECK(argc == 0);
	XFREE(MTYPE_TMP, arr);
	vector_free(v);
}

int main(int argc, char **argv)
{
	test_init_set();
	test_set_index();
	test_lookup();
	test_unset();
	test_unset_value();
	test_remove_compact();
	test_array_conversion();

	if (fail) {
		printf("test_vector: FAILED\n");
		return 1;
	}
	printf("test_vector: OK\n");
	return 0;
}
