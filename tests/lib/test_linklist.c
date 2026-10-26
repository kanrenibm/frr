// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * linklist add/delete/lookup/sort/dup/array tests
 */
#include <zebra.h>

#include "linklist.h"

static int fail;
static int del_count;

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL: line %d: %s\n", __LINE__, #cond);        \
			fail = 1;                                              \
		}                                                              \
	} while (0)

static int v[6] = {0, 1, 2, 3, 4, 5};

static int cmp_int(void *a, void *b)
{
	return *(int *)a - *(int *)b;
}

static int cmp_int_sort(const void **a, const void **b)
{
	return *(const int *)*a - *(const int *)*b;
}

static void del_cb(void *val)
{
	del_count++;
}

/* check list order matches expected indices into v[] */
static bool order_is(struct list *l, const int *idx, unsigned int n)
{
	struct listnode *ln;
	void *data;
	unsigned int i = 0;

	if (listcount(l) != n)
		return false;
	for (ALL_LIST_ELEMENTS_RO(l, ln, data)) {
		if (i >= n || data != &v[idx[i]])
			return false;
		i++;
	}
	return i == n;
}

static void test_add_delete(void)
{
	struct list *l = list_new();
	struct listnode *n;
	static const int a1[] = {1, 2};
	static const int a2[] = {0, 1, 2};
	static const int a3[] = {0, 2};

	CHECK(l != NULL);
	CHECK(list_isempty(l));
	CHECK(listcount(l) == 0);
	CHECK(listhead(l) == NULL);
	CHECK(listtail(l) == NULL);
	CHECK(listnode_head(l) == NULL);

	listnode_add(l, &v[1]);
	listnode_add(l, &v[2]);
	CHECK(order_is(l, a1, 2));

	listnode_add_head(l, &v[0]);
	CHECK(order_is(l, a2, 3));
	CHECK(listnode_head(l) == &v[0]);
	CHECK(listgetdata(listtail(l)) == &v[2]);

	n = listnode_lookup(l, &v[1]);
	CHECK(n != NULL && listgetdata(n) == &v[1]);
	CHECK(listnode_lookup(l, &v[5]) == NULL);

	listnode_delete(l, &v[1]);
	CHECK(order_is(l, a3, 2));
	CHECK(listnode_lookup(l, &v[1]) == NULL);

	/* deleting an absent element is a no-op */
	listnode_delete(l, &v[5]);
	CHECK(listcount(l) == 2);

	list_delete(&l);
	CHECK(l == NULL);
}

static void test_add_before_after_move(void)
{
	struct list *l = list_new();
	struct listnode *n0, *n2;
	static const int a1[] = {0, 2};
	static const int a2[] = {0, 1, 2};
	static const int a3[] = {0, 1, 3, 2};
	static const int a4[] = {0, 1, 3, 2, 4};
	static const int a5[] = {1, 3, 2, 4, 0};

	n0 = listnode_add(l, &v[0]);
	n2 = listnode_add(l, &v[2]);
	CHECK(order_is(l, a1, 2));

	listnode_add_after(l, n0, &v[1]);
	CHECK(order_is(l, a2, 3));

	listnode_add_before(l, n2, &v[3]);
	CHECK(order_is(l, a3, 4));

	/* after the tail updates the tail pointer */
	listnode_add_after(l, listtail(l), &v[4]);
	CHECK(order_is(l, a4, 5));
	CHECK(listgetdata(listtail(l)) == &v[4]);

	listnode_move_to_tail(l, n0);
	CHECK(order_is(l, a5, 5));
	CHECK(listgetdata(listhead(l)) == &v[1]);

	list_delete(&l);
}

static void test_sort(void)
{
	struct list *l = list_new();
	static const int in[] = {3, 1, 4, 0, 2};
	static const int sorted[] = {0, 1, 2, 3, 4};
	static const int sorted_dup[] = {0, 1, 2, 3, 4};
	unsigned int i;

	l->cmp = cmp_int;
	for (i = 0; i < array_size(in); i++)
		listnode_add_sort(l, &v[in[i]]);
	CHECK(order_is(l, sorted, 5));

	/* nodup refuses an element comparing equal to an existing one */
	CHECK(!listnode_add_sort_nodup(l, &v[2]));
	CHECK(listcount(l) == 5);
	CHECK(order_is(l, sorted_dup, 5));
	list_delete(&l);

	/* list_sort on an unsorted list */
	l = list_new();
	for (i = 0; i < array_size(in); i++)
		listnode_add(l, &v[in[i]]);
	list_sort(l, cmp_int_sort);
	CHECK(order_is(l, sorted, 5));
	list_delete(&l);

	/* without cmp, add_sort degenerates to add at the tail */
	l = list_new();
	listnode_add_sort(l, &v[3]);
	listnode_add_sort(l, &v[1]);
	CHECK(listgetdata(listtail(l)) == &v[1]);
	list_delete(&l);
}

static void test_dup_array_force(void)
{
	struct list *l = list_new(), *d, *f = NULL;
	void *arr[3] = {NULL, NULL, NULL};
	void *big[2] = {NULL, NULL};
	static const int a3[] = {0, 1, 2};
	unsigned int i;

	l->cmp = cmp_int;
	l->del = del_cb;
	for (i = 0; i < 3; i++)
		listnode_add(l, &v[i]);

	d = list_dup(l);
	CHECK(d != NULL && d != l);
	CHECK(order_is(d, a3, 3));
	CHECK(d->cmp == l->cmp);
	CHECK(d->del == l->del);

	/* removing from the copy leaves the original alone */
	listnode_delete(d, &v[1]);
	CHECK(listcount(d) == 2 && listcount(l) == 3);

	CHECK(list_to_array(l, arr, 3) == arr);
	CHECK(arr[0] == &v[0] && arr[1] == &v[1] && arr[2] == &v[2]);

	/* array shorter than list: stops at arrlen */
	list_to_array(l, big, 2);
	CHECK(big[0] == &v[0] && big[1] == &v[1]);

	CHECK(listnode_add_force(&f, &v[4]) != NULL);
	CHECK(f != NULL && listcount(f) == 1);
	CHECK(listnode_add_force(&f, &v[5]) != NULL);
	CHECK(listcount(f) == 2);

	list_delete(&f);
	list_delete(&d);
	list_delete(&l);
}

static void test_del_callback(void)
{
	struct list *l = list_new();
	struct listnode *n;
	int i;

	l->del = del_cb;
	del_count = 0;
	for (i = 0; i < 4; i++)
		listnode_add(l, &v[i]);

	/* listnode_delete and list_delete_node do not call it */
	listnode_delete(l, &v[0]);
	CHECK(del_count == 0);
	n = listhead(l);
	list_delete_node(l, n);
	CHECK(del_count == 0);
	CHECK(listcount(l) == 2);

	/* delete_all_node and list_delete do */
	list_delete_all_node(l);
	CHECK(del_count == 2);
	CHECK(list_isempty(l));
	CHECK(listcount(l) == 0);

	listnode_add(l, &v[4]);
	listnode_add(l, &v[5]);
	del_count = 0;
	list_delete(&l);
	CHECK(del_count == 2);
}

int main(int argc, char **argv)
{
	test_add_delete();
	test_add_before_after_move();
	test_sort();
	test_dup_array_force();
	test_del_callback();

	if (fail)
		return 1;
	printf("OK\n");
	return 0;
}
