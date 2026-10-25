// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * admin_group set/get/compare/copy and match_any/match_all tests
 */
#include <zebra.h>

#include "admin_group.h"

static int fail;

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL: line %d: %s\n", __LINE__, #cond);        \
			fail = 1;                                              \
		}                                                              \
	} while (0)

static void test_basic(void)
{
	struct admin_group ag = {};

	admin_group_init(&ag);
	CHECK(admin_group_zero(&ag));
	CHECK(admin_group_size(&ag) == 0);
	CHECK(admin_group_nb_words(&ag) == 0);
	CHECK(admin_group_get(&ag, 5) == 0);

	admin_group_set(&ag, 5);
	CHECK(admin_group_get(&ag, 5) == 1);
	CHECK(admin_group_get(&ag, 4) == 0);
	CHECK(admin_group_get_offset(&ag, 0) == (1u << 5));
	CHECK(admin_group_size(&ag) == 1);
	CHECK(admin_group_nb_words(&ag) == 1);
	CHECK(!admin_group_zero(&ag));

	/* bit 40 lives in the second word, forcing the bitmap to grow */
	admin_group_set(&ag, 40);
	CHECK(admin_group_get_offset(&ag, 1) == (1u << 8));
	CHECK(admin_group_size(&ag) == 2);
	CHECK(admin_group_nb_words(&ag) == 2);

	admin_group_unset(&ag, 40);
	CHECK(admin_group_size(&ag) == 1);
	CHECK(admin_group_get_offset(&ag, 1) == 0);
	admin_group_unset(&ag, 5);
	CHECK(admin_group_get(&ag, 5) == 0);
	CHECK(admin_group_zero(&ag));
	CHECK(admin_group_size(&ag) == 0);

	admin_group_bulk_set(&ag, 0xff, 2);
	CHECK(admin_group_get_offset(&ag, 2) == 0xff);
	CHECK(admin_group_get_offset(&ag, 1) == 0);
	CHECK(admin_group_size(&ag) == 3);

	admin_group_clear(&ag);
	CHECK(admin_group_zero(&ag));
	CHECK(admin_group_nb_words(&ag) == 0);

	admin_group_term(&ag);
}

static void test_explicit_zero(void)
{
	struct admin_group ag = {};

	admin_group_init(&ag);
	CHECK(!admin_group_explicit_zero(&ag));

	/* bulk_set of an all-zero first word requests an explicit zero */
	admin_group_bulk_set(&ag, 0, 0);
	CHECK(admin_group_zero(&ag));
	CHECK(admin_group_explicit_zero(&ag));
	CHECK(admin_group_nb_words(&ag) == 1);

	admin_group_disallow_explicit_zero(&ag);
	CHECK(!admin_group_explicit_zero(&ag));
	CHECK(admin_group_nb_words(&ag) == 0);

	admin_group_allow_explicit_zero(&ag);
	CHECK(admin_group_explicit_zero(&ag));

	/* a non-zero group is never turned into an explicit zero */
	admin_group_clear(&ag);
	admin_group_set(&ag, 1);
	admin_group_allow_explicit_zero(&ag);
	CHECK(!admin_group_explicit_zero(&ag));
	admin_group_disallow_explicit_zero(&ag);
	CHECK(admin_group_nb_words(&ag) == 1);

	admin_group_term(&ag);
}

static void test_cmp_copy(void)
{
	struct admin_group a = {}, b = {}, c = {};

	admin_group_init(&a);
	admin_group_init(&b);
	admin_group_init(&c);

	CHECK(admin_group_cmp(&a, &b));

	admin_group_set(&a, 3);
	CHECK(!admin_group_cmp(&a, &b));
	CHECK(!admin_group_cmp(&b, &a));
	admin_group_set(&b, 3);
	CHECK(admin_group_cmp(&a, &b));

	/* trailing zero words must not affect the comparison */
	admin_group_set(&a, 70);
	CHECK(!admin_group_cmp(&a, &b));
	CHECK(!admin_group_cmp(&b, &a));
	admin_group_unset(&a, 70);
	CHECK(admin_group_cmp(&a, &b));
	CHECK(admin_group_cmp(&b, &a));

	admin_group_set(&a, 33);
	admin_group_copy(&c, &a);
	CHECK(admin_group_cmp(&a, &c));
	CHECK(admin_group_size(&c) == 2);

	/* the copy is independent of the source */
	admin_group_set(&a, 9);
	CHECK(!admin_group_cmp(&a, &c));

	admin_group_term(&a);
	admin_group_term(&b);
	admin_group_term(&c);
}

static void test_match(void)
{
	struct admin_group fad = {}, ext = {};
	uint32_t std;

	admin_group_init(&fad);
	admin_group_init(&ext);

	/* empty constraint with no link groups: nothing to match any of,
	 * nothing required for all
	 */
	CHECK(!admin_group_match_any(&fad, NULL, NULL));
	CHECK(admin_group_match_all(&fad, NULL, NULL));

	/* standard (32 bit) link group only */
	admin_group_bulk_set(&fad, 0x3, 0);
	std = 0x1;
	CHECK(admin_group_match_any(&fad, &std, NULL));
	CHECK(!admin_group_match_all(&fad, &std, NULL));
	std = 0x3;
	CHECK(admin_group_match_all(&fad, &std, NULL));
	std = 0x4;
	CHECK(!admin_group_match_any(&fad, &std, NULL));
	CHECK(!admin_group_match_any(&fad, NULL, NULL));
	CHECK(!admin_group_match_all(&fad, NULL, NULL));

	/* extended link group only, second word */
	admin_group_clear(&fad);
	admin_group_bulk_set(&fad, 0x10, 1);
	admin_group_bulk_set(&ext, 0x10, 1);
	CHECK(admin_group_match_any(&fad, NULL, &ext));
	CHECK(admin_group_match_all(&fad, NULL, &ext));
	admin_group_clear(&ext);
	admin_group_bulk_set(&ext, 0x20, 1);
	CHECK(!admin_group_match_any(&fad, NULL, &ext));
	CHECK(!admin_group_match_all(&fad, NULL, &ext));

	/* a constraint wider than the link group can never match_all */
	admin_group_clear(&ext);
	admin_group_set(&ext, 1);
	CHECK(!admin_group_match_all(&fad, NULL, &ext));

	/* RFC7308: with both present the standard group supplies word 0 */
	admin_group_clear(&fad);
	admin_group_clear(&ext);
	admin_group_set(&fad, 0);
	admin_group_set(&ext, 1);
	std = 0x1;
	CHECK(admin_group_match_any(&fad, &std, &ext));
	CHECK(admin_group_match_all(&fad, &std, &ext));
	std = 0x2;
	admin_group_clear(&ext);
	admin_group_set(&ext, 0);
	CHECK(!admin_group_match_any(&fad, &std, &ext));
	CHECK(!admin_group_match_all(&fad, &std, &ext));

	admin_group_term(&fad);
	admin_group_term(&ext);
}

int main(int argc, char **argv)
{
	test_basic();
	test_explicit_zero();
	test_cmp_copy();
	test_match();

	printf("final tally: %s\n", fail ? "FAILED" : "ok");
	return fail;
}
