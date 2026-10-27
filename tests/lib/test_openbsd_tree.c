// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * lib/openbsd-tree RB tree and SPLAY tree tests
 */
#include <zebra.h>

#include "openbsd-tree.h"

static int fail;

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL: line %d: %s\n", __LINE__, #cond);        \
			fail = 1;                                              \
		}                                                              \
	} while (0)

#define NNODES 200

struct node {
	RB_ENTRY(node) rb;
	SPLAY_ENTRY(node) sp;
	int key;
};

RB_HEAD(rbt, node);
SPLAY_HEAD(spt, node);

static int node_cmp(const struct node *a, const struct node *b)
{
	return (a->key > b->key) - (a->key < b->key);
}

RB_PROTOTYPE(rbt, node, rb, node_cmp);
RB_GENERATE(rbt, node, rb, node_cmp);
SPLAY_PROTOTYPE(spt, node, sp, node_cmp);
SPLAY_GENERATE(spt, node, sp, node_cmp);

/* deterministic permutation of 0..NNODES-1 (7 is coprime with 200) */
static int perm(int i)
{
	return (i * 7 + 3) % NNODES;
}

static void test_rb_basic(void)
{
	struct rbt head;
	struct node n[3], dup, probe;

	RB_INIT(rbt, &head);
	CHECK(RB_EMPTY(rbt, &head));
	CHECK(RB_ROOT(rbt, &head) == NULL);
	CHECK(RB_MIN(rbt, &head) == NULL);
	CHECK(RB_MAX(rbt, &head) == NULL);

	n[0].key = 10;
	n[1].key = 20;
	n[2].key = 30;
	CHECK(RB_INSERT(rbt, &head, &n[1]) == NULL);
	CHECK(RB_INSERT(rbt, &head, &n[0]) == NULL);
	CHECK(RB_INSERT(rbt, &head, &n[2]) == NULL);
	CHECK(!RB_EMPTY(rbt, &head));

	/* duplicate key: existing element returned, tree unchanged */
	dup.key = 20;
	CHECK(RB_INSERT(rbt, &head, &dup) == &n[1]);

	CHECK(RB_MIN(rbt, &head) == &n[0]);
	CHECK(RB_MAX(rbt, &head) == &n[2]);
	CHECK(RB_NEXT(rbt, &n[0]) == &n[1]);
	CHECK(RB_NEXT(rbt, &n[1]) == &n[2]);
	CHECK(RB_NEXT(rbt, &n[2]) == NULL);
	CHECK(RB_PREV(rbt, &n[2]) == &n[1]);
	CHECK(RB_PREV(rbt, &n[0]) == NULL);

	probe.key = 20;
	CHECK(RB_FIND(rbt, &head, &probe) == &n[1]);
	probe.key = 25;
	CHECK(RB_FIND(rbt, &head, &probe) == NULL);

	/* NFIND returns the smallest element >= key */
	CHECK(RB_NFIND(rbt, &head, &probe) == &n[2]);
	probe.key = 5;
	CHECK(RB_NFIND(rbt, &head, &probe) == &n[0]);
	probe.key = 31;
	CHECK(RB_NFIND(rbt, &head, &probe) == NULL);

	CHECK(RB_REMOVE(rbt, &head, &n[1]) == &n[1]);
	probe.key = 20;
	CHECK(RB_FIND(rbt, &head, &probe) == NULL);
	CHECK(RB_NEXT(rbt, &n[0]) == &n[2]);

	CHECK(RB_REMOVE(rbt, &head, &n[0]) == &n[0]);
	CHECK(RB_REMOVE(rbt, &head, &n[2]) == &n[2]);
	CHECK(RB_EMPTY(rbt, &head));
}

static void test_rb_many(void)
{
	static struct node n[NNODES];
	struct rbt head;
	struct node *p, *tmp;
	int i, expect;

	RB_INIT(rbt, &head);
	for (i = 0; i < NNODES; i++) {
		n[perm(i)].key = perm(i);
		CHECK(RB_INSERT(rbt, &head, &n[perm(i)]) == NULL);
	}

	/* forward iteration is sorted and complete */
	expect = 0;
	RB_FOREACH (p, rbt, &head)
		CHECK(p->key == expect++);
	CHECK(expect == NNODES);

	/* reverse iteration */
	expect = NNODES - 1;
	RB_FOREACH_REVERSE (p, rbt, &head)
		CHECK(p->key == expect--);
	CHECK(expect == -1);

	/* remove the even keys while iterating */
	RB_FOREACH_SAFE (p, rbt, &head, tmp) {
		if (p->key % 2 == 0)
			CHECK(RB_REMOVE(rbt, &head, p) == p);
	}
	expect = 1;
	RB_FOREACH (p, rbt, &head) {
		CHECK(p->key == expect);
		expect += 2;
	}
	CHECK(expect == NNODES + 1);

	/* drain in reverse order with the safe iterator */
	RB_FOREACH_REVERSE_SAFE (p, rbt, &head, tmp)
		CHECK(RB_REMOVE(rbt, &head, p) == p);
	CHECK(RB_EMPTY(rbt, &head));
}

static void test_splay(void)
{
	static struct node n[NNODES];
	struct spt head;
	struct node *p, probe;
	int i, expect;

	SPLAY_INIT(&head);
	CHECK(SPLAY_EMPTY(&head));
	CHECK(SPLAY_MIN(spt, &head) == NULL);
	CHECK(SPLAY_MAX(spt, &head) == NULL);

	for (i = 0; i < NNODES; i++) {
		n[perm(i)].key = perm(i);
		CHECK(SPLAY_INSERT(spt, &head, &n[perm(i)]) == NULL);
	}
	CHECK(!SPLAY_EMPTY(&head));

	/* duplicate key returns the existing element */
	probe.key = 42;
	CHECK(SPLAY_INSERT(spt, &head, &probe) == &n[42]);

	CHECK(SPLAY_MIN(spt, &head) == &n[0]);
	CHECK(SPLAY_MAX(spt, &head) == &n[NNODES - 1]);

	probe.key = 77;
	CHECK(SPLAY_FIND(spt, &head, &probe) == &n[77]);
	probe.key = NNODES + 5;
	CHECK(SPLAY_FIND(spt, &head, &probe) == NULL);

	expect = 0;
	SPLAY_FOREACH (p, spt, &head)
		CHECK(p->key == expect++);
	CHECK(expect == NNODES);

	/* remove odd keys, remaining order must stay sorted */
	for (i = 1; i < NNODES; i += 2)
		CHECK(SPLAY_REMOVE(spt, &head, &n[i]) == &n[i]);
	probe.key = 1;
	CHECK(SPLAY_FIND(spt, &head, &probe) == NULL);
	expect = 0;
	SPLAY_FOREACH (p, spt, &head) {
		CHECK(p->key == expect);
		expect += 2;
	}
	CHECK(expect == NNODES);

	/* removing an absent key leaves the tree intact */
	CHECK(SPLAY_REMOVE(spt, &head, &probe) == NULL);
	CHECK(SPLAY_MIN(spt, &head) == &n[0]);
}

int main(int argc, char **argv)
{
	test_rb_basic();
	test_rb_many();
	test_splay();

	if (!fail)
		printf("openbsd-tree tests passed\n");
	return fail;
}
