// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * hash create, get, lookup, release, iterate, walk, expand and clean tests
 */
#include <zebra.h>

#include "hash.h"
#include "linklist.h"

static int fail;

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL: line %d: %s\n", __LINE__, #cond);        \
			fail = 1;                                              \
		}                                                              \
	} while (0)

struct item {
	unsigned int id;
};

static unsigned int item_key(const void *data)
{
	return ((const struct item *)data)->id;
}

static bool item_cmp(const void *a, const void *b)
{
	return ((const struct item *)a)->id == ((const struct item *)b)->id;
}

static void sum_iter(struct hash_bucket *hb, void *arg)
{
	*(unsigned long *)arg += ((struct item *)hb->data)->id;
}

static int walk_abort_after_three(struct hash_bucket *hb, void *arg)
{
	int *visited = arg;

	(*visited)++;
	return *visited == 3 ? HASHWALK_ABORT : HASHWALK_CONTINUE;
}

static unsigned int freed;

static void count_free(void *data)
{
	(void)data;
	freed++;
}

static void test_basic(void)
{
	struct item items[4] = {{1}, {2}, {3}, {4}};
	struct item missing = {99};
	struct hash *h = hash_create_size(8, item_key, item_cmp, "test");
	unsigned long sum = 0;
	int i;

	CHECK(h != NULL);
	CHECK(hashcount(h) == 0);
	CHECK(h->size == 8);

	/* lookup in an empty table without an alloc function */
	CHECK(hash_lookup(h, &missing) == NULL);
	CHECK(hash_get(h, &missing, NULL) == NULL);

	for (i = 0; i < 4; i++)
		CHECK(hash_get(h, &items[i], hash_alloc_intern) == &items[i]);
	CHECK(hashcount(h) == 4);

	/* a second get with an equal key returns the existing entry */
	struct item dup = {2};

	CHECK(hash_get(h, &dup, hash_alloc_intern) == &items[1]);
	CHECK(hashcount(h) == 4);

	for (i = 0; i < 4; i++)
		CHECK(hash_lookup(h, &items[i]) == &items[i]);
	CHECK(hash_lookup(h, &missing) == NULL);
	CHECK(hashcount(h) == 4);

	hash_iterate(h, sum_iter, &sum);
	CHECK(sum == 10);

	/* release returns the data and removes it */
	CHECK(hash_release(h, &dup) == &items[1]);
	CHECK(hashcount(h) == 3);
	CHECK(hash_lookup(h, &dup) == NULL);
	CHECK(hash_release(h, &dup) == NULL);
	CHECK(hashcount(h) == 3);

	hash_clean_and_free(&h, NULL);
	CHECK(h == NULL);
}

static void *null_alloc(void *data)
{
	(void)data;
	return NULL;
}

static void test_alloc_null(void)
{
	struct item it = {5};
	struct hash *h = hash_create(item_key, item_cmp, NULL);

	/* an alloc function returning NULL inserts nothing */
	CHECK(hash_get(h, &it, null_alloc) == NULL);
	CHECK(hashcount(h) == 0);

	hash_clean_and_free(&h, NULL);
}

static void test_expand_and_list(void)
{
	enum { N = 40 };
	static struct item items[N];
	struct hash *h = hash_create_size(4, item_key, item_cmp, "expand");
	struct list *l;
	unsigned long sum = 0;
	unsigned int i;

	for (i = 0; i < N; i++) {
		items[i].id = i + 1;
		CHECK(hash_get(h, &items[i], hash_alloc_intern) == &items[i]);
	}
	CHECK(hashcount(h) == N);
	/* growth past the threshold must have enlarged the table */
	CHECK(h->size > 4);

	for (i = 0; i < N; i++)
		CHECK(hash_lookup(h, &items[i]) == &items[i]);

	hash_iterate(h, sum_iter, &sum);
	CHECK(sum == (unsigned long)N * (N + 1) / 2);

	l = hash_to_list(h);
	CHECK(listcount(l) == N);
	list_delete(&l);

	hash_clean_and_free(&h, NULL);
}

static void test_walk_and_clean(void)
{
	static struct item items[10];
	struct hash *h = hash_create(item_key, item_cmp, "walk");
	int visited = 0;
	unsigned int i;

	for (i = 0; i < 10; i++) {
		items[i].id = i + 1;
		hash_get(h, &items[i], hash_alloc_intern);
	}

	hash_walk(h, walk_abort_after_three, &visited);
	CHECK(visited == 3);

	freed = 0;
	hash_clean(h, count_free);
	CHECK(freed == 10);
	CHECK(hashcount(h) == 0);
	CHECK(hash_lookup(h, &items[0]) == NULL);

	/* table is reusable after hash_clean */
	CHECK(hash_get(h, &items[0], hash_alloc_intern) == &items[0]);
	CHECK(hashcount(h) == 1);

	freed = 0;
	hash_clean_and_free(&h, count_free);
	CHECK(freed == 1);
	CHECK(h == NULL);
}

static void test_string_hash(void)
{
	CHECK(string_hash_make("") == 0);
	CHECK(string_hash_make("a") == (unsigned int)'a');
	CHECK(string_hash_make("ab") == (((unsigned int)'a' * 33) ^ 'b'));
	CHECK(string_hash_make("abc") == string_hash_make("abc"));
	CHECK(string_hash_make("abc") != string_hash_make("abd"));
}

int main(void)
{
	test_basic();
	test_alloc_null();
	test_expand_and_list();
	test_walk_and_clean();
	test_string_hash();

	if (fail)
		return 1;
	printf("OK\n");
	return 0;
}
