// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * lib/json.c helper tests: use_json, typed adders and printf-style adders
 */
#include <zebra.h>

#include "json.h"

static int fail;

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL: line %d: %s\n", __LINE__, #cond);        \
			fail = 1;                                              \
		}                                                              \
	} while (0)

static const char *str_of(struct json_object *obj, const char *key)
{
	struct json_object *v;

	if (!json_object_object_get_ex(obj, key, &v))
		return NULL;
	return json_object_get_string(v);
}

static void test_use_json(void)
{
	struct cmd_token t_show = { .text = (char *)"show", .arg = (char *)"show" };
	struct cmd_token t_json = { .text = (char *)"json", .arg = (char *)"json" };
	struct cmd_token t_noarg = { .text = (char *)"json", .arg = NULL };
	struct cmd_token t_other = { .text = (char *)"detail", .arg = (char *)"detail" };
	struct cmd_token *argv[3];

	CHECK(!use_json(0, argv));

	argv[0] = &t_show;
	argv[1] = &t_json;
	CHECK(use_json(2, argv));

	/* json must be the last keyword */
	argv[0] = &t_json;
	argv[1] = &t_other;
	CHECK(!use_json(2, argv));

	/* a token without user input never matches */
	argv[0] = &t_noarg;
	CHECK(!use_json(1, argv));
}

static void test_adders(void)
{
	struct json_object *obj = json_object_new_object();
	struct json_object *v;

	json_object_string_add(obj, "s", "hello");
	json_object_int_add(obj, "i", -5000000000LL);
	json_object_double_add(obj, "d", 1.5);
	json_object_boolean_true_add(obj, "t");
	json_object_boolean_false_add(obj, "f");
	json_object_boolean_add(obj, "b", true);

	CHECK(!strcmp(str_of(obj, "s"), "hello"));

	CHECK(json_object_object_get_ex(obj, "i", &v));
	CHECK(json_object_get_int64(v) == -5000000000LL);

	CHECK(json_object_object_get_ex(obj, "d", &v));
	CHECK(json_object_get_double(v) == 1.5);

	CHECK(json_object_object_get_ex(obj, "t", &v));
	CHECK(json_object_get_boolean(v));
	CHECK(json_object_object_get_ex(obj, "f", &v));
	CHECK(!json_object_get_boolean(v));
	CHECK(json_object_object_get_ex(obj, "b", &v));
	CHECK(json_object_get_boolean(v));

	json_object_free(obj);
}

static void test_printf_adders(void)
{
	struct json_object *obj = json_object_new_object();
	struct json_object *arr = json_object_new_array();
	struct json_object *child;
	char longstr[400];
	struct json_object *s;

	json_object_string_addf(obj, "k", "%s-%d", "abc", 7);
	CHECK(!strcmp(str_of(obj, "k"), "abc-7"));

	json_array_string_add(arr, "plain");
	json_array_string_addf(arr, "n=%d", 3);
	CHECK(json_object_array_length(arr) == 2);
	CHECK(!strcmp(json_object_get_string(json_object_array_get_idx(arr, 0)),
		      "plain"));
	CHECK(!strcmp(json_object_get_string(json_object_array_get_idx(arr, 1)),
		      "n=3"));

	child = json_object_new_object();
	json_object_object_addf(obj, child, "child-%d", 9);
	CHECK(json_object_object_get_ex(obj, "child-9", &child));

	/* output longer than the 256-byte stack buffer takes the heap path */
	memset(longstr, 'x', sizeof(longstr) - 1);
	longstr[sizeof(longstr) - 1] = '\0';
	s = json_object_new_stringf("%s", longstr);
	CHECK(json_object_get_string_len(s) == sizeof(longstr) - 1);
	CHECK(!strcmp(json_object_get_string(s), longstr));
	json_object_free(s);

	json_object_free(arr);
	json_object_free(obj);
}

static void test_lock(void)
{
	struct json_object *obj = json_object_new_object();

	/* lock takes an extra reference, so two frees are needed */
	CHECK(json_object_lock(obj) == obj);
	json_object_free(obj);
	json_object_string_add(obj, "still", "alive");
	CHECK(!strcmp(str_of(obj, "still"), "alive"));
	json_object_free(obj);
}

int main(void)
{
	test_use_json();
	test_adders();
	test_printf_adders();
	test_lock();

	if (fail)
		return 1;
	printf("OK\n");
	return 0;
}
