// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * agg_table wrapper tests (lib/agg_table.c, lib/agg_table.h)
 */
#include <zebra.h>

#include "agg_table.h"

static int fail;

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL: line %d: %s\n", __LINE__, #cond);        \
			fail = 1;                                              \
		}                                                              \
	} while (0)

static void test_init_info(void)
{
	struct agg_table *at = agg_table_init();
	int marker = 0;

	CHECK(at != NULL);
	CHECK(at->route_table != NULL);
	CHECK(agg_get_table_info(at) == NULL);
	CHECK(agg_route_top(at) == NULL);

	agg_set_table_info(at, &marker);
	CHECK(agg_get_table_info(at) == &marker);

	agg_table_finish(at);
}

static void test_get_lookup_match(void)
{
	struct agg_table *at = agg_table_init();
	struct prefix p8, p16, p24, host;
	struct agg_node *n8, *n16, *n24, *found;
	int tag = 7;

	str2prefix("10.0.0.0/8", &p8);
	str2prefix("10.1.0.0/16", &p16);
	str2prefix("10.1.2.0/24", &p24);
	str2prefix("10.1.2.3/32", &host);

	CHECK(agg_node_lookup(at, &p8) == NULL);

	n8 = agg_node_get(at, &p8);
	n16 = agg_node_get(at, &p16);
	n24 = agg_node_get(at, &p24);
	CHECK(n8 && n16 && n24);

	/* get on an existing prefix returns the same node */
	CHECK(agg_node_get(at, &p16) == n16);
	agg_unlock_node(n16);

	/* aggregate pointer is per node */
	n16->aggregate = &tag;
	CHECK(n8->aggregate == NULL);
	CHECK(n16->aggregate == &tag);

	/* exact lookup: hits stored prefixes, misses others */
	found = agg_node_lookup(at, &p24);
	CHECK(found == n24);
	agg_unlock_node(found);
	CHECK(agg_node_lookup(at, &host) == NULL);

	/* longest-prefix match */
	found = agg_node_match(at, &host);
	CHECK(found == n24);
	agg_unlock_node(found);

	/* back-pointers and prefix accessor */
	CHECK(agg_get_table(n24) == at);
	CHECK(prefix_same(agg_node_get_prefix(n24), &p24));
	CHECK(agg_node_parent(n24) == n16);
	CHECK(agg_node_parent(n16) == n8);

	/* release in leaf-to-root order; unreferenced empty nodes are freed */
	agg_unlock_node(n24);
	agg_unlock_node(n16);
	agg_unlock_node(n8);
	CHECK(agg_route_top(at) == NULL);

	agg_table_finish(at);
}

static void test_lock_count(void)
{
	struct agg_table *at = agg_table_init();
	struct prefix p;
	struct agg_node *n;
	unsigned int base;

	str2prefix("192.0.2.0/24", &p);
	n = agg_node_get(at, &p);
	base = agg_node_get_lock_count(n);
	CHECK(base >= 1);

	agg_lock_node(n);
	CHECK(agg_node_get_lock_count(n) == base + 1);
	agg_unlock_node(n);
	CHECK(agg_node_get_lock_count(n) == base);

	agg_unlock_node(n);
	agg_table_finish(at);
}

static void test_iteration(void)
{
	struct agg_table *at = agg_table_init();
	const char *strs[] = {"10.0.0.0/8", "10.1.0.0/16", "172.16.0.0/12"};
	struct agg_node *held[array_size(strs)];
	struct agg_node *n, *it;
	struct prefix p;
	size_t i;
	int seen = 0, sub = 0;

	/* keep the references so the nodes are retained */
	for (i = 0; i < array_size(strs); i++) {
		str2prefix(strs[i], &p);
		held[i] = agg_node_get(at, &p);
		CHECK(held[i] != NULL);
	}

	for (n = agg_route_top(at); n; n = agg_route_next(n)) {
		for (i = 0; i < array_size(strs); i++)
			if (n == held[i])
				seen++;
	}
	CHECK(seen == 3);

	/* a walk limited to the 10.0.0.0/8 subtree sees 10.1.0.0/16 only */
	n = agg_route_table_top(held[0]);
	CHECK(n != NULL);
	agg_unlock_node(n);

	for (it = agg_route_next_until(agg_lock_node(held[0]), held[0]); it;
	     it = agg_route_next_until(it, held[0])) {
		CHECK(it == held[1]);
		sub++;
	}
	CHECK(sub == 1);

	for (i = array_size(strs); i > 0; i--)
		agg_unlock_node(held[i - 1]);
	agg_table_finish(at);
}

int main(int argc, char **argv)
{
	test_init_info();
	test_get_lookup_match();
	test_lock_count();
	test_iteration();

	if (fail) {
		printf("agg_table tests FAILED\n");
		return 1;
	}
	printf("agg_table tests passed\n");
	return 0;
}
