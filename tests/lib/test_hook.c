// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * hook register/unregister/call ordering tests (lib/hook.c)
 */
#include <zebra.h>

#include "hook.h"

static int fail;

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL: line %d: %s\n", __LINE__, #cond);        \
			fail = 1;                                              \
		}                                                              \
	} while (0)

DECLARE_HOOK(test_hook, (int x), (x));
DEFINE_HOOK(test_hook, (int x), (x));
DECLARE_KOOH(test_kooh, (int x), (x));
DEFINE_KOOH(test_kooh, (int x), (x));

static char trace[64];

static void note(char c)
{
	size_t len = strlen(trace);

	if (len < sizeof(trace) - 1) {
		trace[len] = c;
		trace[len + 1] = '\0';
	}
}

static int hook_a(int x)
{
	note('a');
	return x;
}

static int hook_b(int x)
{
	note('b');
	return x * 10;
}

static int hook_c(int x)
{
	note('c');
	return x * 100;
}

static int hook_arg(void *arg, int x)
{
	note(*(char *)arg);
	return x;
}

static int kooh_a(int x)
{
	note('a');
	return x;
}

static int kooh_b(int x)
{
	note('b');
	return x;
}

static int kooh_c(int x)
{
	note('c');
	return x;
}

static void test_empty(void)
{
	CHECK(!hook_have_hooks(test_hook));
	CHECK(hook_call(test_hook, 1) == 0);
}

static void test_priority_order(void)
{
	trace[0] = '\0';
	hook_register_prio(test_hook, 20, hook_a);
	hook_register_prio(test_hook, 10, hook_b);
	hook_register_prio(test_hook, 30, hook_c);
	CHECK(hook_have_hooks(test_hook));

	/* ascending priority; return values are summed */
	CHECK(hook_call(test_hook, 1) == 1 + 10 + 100);
	CHECK(strcmp(trace, "bac") == 0);

	trace[0] = '\0';
	hook_unregister(test_hook, hook_b);
	CHECK(hook_call(test_hook, 2) == 2 + 200);
	CHECK(strcmp(trace, "ac") == 0);

	hook_unregister(test_hook, hook_a);
	hook_unregister(test_hook, hook_c);
	CHECK(!hook_have_hooks(test_hook));
	CHECK(hook_call(test_hook, 1) == 0);
}

static void test_equal_priority(void)
{
	trace[0] = '\0';
	hook_register(test_hook, hook_a);
	hook_register(test_hook, hook_b);
	hook_register(test_hook, hook_c);

	/* equal priority: the most recently registered hook runs first */
	CHECK(hook_call(test_hook, 1) == 111);
	CHECK(strcmp(trace, "cba") == 0);

	hook_unregister(test_hook, hook_a);
	hook_unregister(test_hook, hook_b);
	hook_unregister(test_hook, hook_c);
	CHECK(!hook_have_hooks(test_hook));
}

static void test_arg(void)
{
	char ca = 'x', cb = 'y';

	trace[0] = '\0';
	hook_register_arg(test_hook, hook_arg, &ca);
	hook_register_arg_prio(test_hook, 5, hook_arg, &cb);

	CHECK(hook_call(test_hook, 3) == 6);
	CHECK(strcmp(trace, "yx") == 0);

	/* wrong arg: nothing removed */
	hook_unregister_arg(test_hook, hook_arg, NULL);
	CHECK(hook_have_hooks(test_hook));

	hook_register(test_hook, hook_a);
	CHECK(hook_call(test_hook, 1) == 3);

	trace[0] = '\0';
	hook_unregister_arg(test_hook, hook_arg, &cb);
	CHECK(hook_call(test_hook, 1) == 2);
	CHECK(strcmp(trace, "ax") == 0 || strcmp(trace, "xa") == 0);

	hook_unregister_arg(test_hook, hook_arg, &ca);
	hook_unregister(test_hook, hook_a);
	CHECK(!hook_have_hooks(test_hook));
}

static void test_heap_entries(void)
{
	int i;

	/* the same call site registered repeatedly exhausts its static
	 * hookent and must fall back to heap-allocated entries
	 */
	trace[0] = '\0';
	for (i = 0; i < 3; i++)
		hook_register_prio(test_hook, i, hook_a);
	CHECK(hook_call(test_hook, 1) == 3);
	CHECK(strcmp(trace, "aaa") == 0);

	for (i = 0; i < 3; i++)
		hook_unregister(test_hook, hook_a);
	CHECK(!hook_have_hooks(test_hook));

	/* unregistering something not registered is harmless */
	hook_unregister(test_hook, hook_a);
	CHECK(!hook_have_hooks(test_hook));
}

static void test_kooh(void)
{
	trace[0] = '\0';
	hook_register_prio(test_kooh, 10, kooh_a);
	hook_register_prio(test_kooh, 30, kooh_b);
	hook_register_prio(test_kooh, 20, kooh_c);

	/* reverse hooks run in descending priority */
	hook_call(test_kooh, 0);
	CHECK(strcmp(trace, "bca") == 0);

	hook_unregister(test_kooh, kooh_a);
	hook_unregister(test_kooh, kooh_b);
	hook_unregister(test_kooh, kooh_c);
	CHECK(!hook_have_hooks(test_kooh));

	/* equal priority: registration order is preserved */
	trace[0] = '\0';
	hook_register(test_kooh, kooh_a);
	hook_register(test_kooh, kooh_b);
	hook_register(test_kooh, kooh_c);
	hook_call(test_kooh, 0);
	CHECK(strcmp(trace, "abc") == 0);

	hook_unregister(test_kooh, kooh_a);
	hook_unregister(test_kooh, kooh_b);
	hook_unregister(test_kooh, kooh_c);
}

int main(int argc, char **argv)
{
	test_empty();
	test_priority_order();
	test_equal_priority();
	test_arg();
	test_heap_entries();
	test_kooh();

	if (fail) {
		printf("hook tests FAILED\n");
		return 1;
	}
	printf("hook tests passed\n");
	return 0;
}
