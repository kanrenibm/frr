// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * bitfield assign/release/test/find/iterate tests
 */
#include <zebra.h>

#include "bitfield.h"

static int fail;

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL: line %d: %s\n", __LINE__, #cond);        \
			fail = 1;                                              \
		}                                                              \
	} while (0)

static void test_assign_release(void)
{
	bitfield_t bf;
	unsigned int id, a, b, c;

	bf_init(bf, 32);
	CHECK(bf_is_inited(bf));

	/* ids are handed out lowest-first */
	for (id = 0; id < 40; id++) {
		unsigned int got;

		bf_assign_index(bf, got);
		CHECK(got == id);
		CHECK(bf_test_index(bf, got));
	}

	/* a released id is the next one reused */
	bf_release_index(bf, 5);
	CHECK(!bf_test_index(bf, 5));
	CHECK(bf_test_index(bf, 4));
	CHECK(bf_test_index(bf, 6));
	bf_assign_index(bf, a);
	CHECK(a == 5);

	/* lowest released id wins when several are free */
	bf_release_index(bf, 33);
	bf_release_index(bf, 2);
	bf_release_index(bf, 17);
	bf_assign_index(bf, a);
	bf_assign_index(bf, b);
	bf_assign_index(bf, c);
	CHECK(a == 2);
	CHECK(b == 17);
	CHECK(c == 33);

	bf_free(bf);
	CHECK(!bf_is_inited(bf));
}

static void test_zero_index(void)
{
	bitfield_t bf;
	unsigned int id;

	bf_init(bf, 8);
	bf_assign_zero_index(bf);
	CHECK(bf_test_index(bf, 0));
	bf_assign_index(bf, id);
	CHECK(id == 1);
	bf_release_zero_index(bf);
	CHECK(!bf_test_index(bf, 0));
	bf_assign_index(bf, id);
	CHECK(id == 0);
	bf_free(bf);
}

static void test_cmp(void)
{
	bitfield_t x, y;
	unsigned int id;

	bf_init(x, 64);
	bf_init(y, 64);
	CHECK(bf_cmp(x, y) == 0);

	bf_assign_index(x, id);
	CHECK(bf_cmp(x, y) != 0);
	bf_assign_index(y, id);
	CHECK(bf_cmp(x, y) == 0);

	bf_free(x);
	bf_free(y);
}

static void test_set_bit_growth(void)
{
	bitfield_t bf;
	size_t m0;
	unsigned int i;

	bf_init(bf, 0);
	CHECK(bf.m == 1);
	m0 = bf.m;

	/* filling the last word makes the bitfield grow by one word */
	for (i = 0; i < WORD_SIZE; i++)
		bf_set_bit(bf, i);
	CHECK(bf.n == 1);
	CHECK(bf.m == m0 + 1);
	CHECK(bf.data[0] == WORD_MAX);
	CHECK(bf.data[1] == 0);

	bf_free(bf);
}

static void test_find_next_set_bit(void)
{
	bitfield_t bf;
	unsigned int b, seen[4], n = 0;

	bf_init(bf, 96);
	CHECK(bf_find_next_set_bit(bf, 0) == WORD_MAX);

	bf_set_bit(bf, 3);
	bf_set_bit(bf, 31);
	bf_set_bit(bf, 32);
	bf_set_bit(bf, 70);

	CHECK(bf_find_next_set_bit(bf, 0) == 3);
	CHECK(bf_find_next_set_bit(bf, 3) == 3);
	CHECK(bf_find_next_set_bit(bf, 4) == 31);
	CHECK(bf_find_next_set_bit(bf, 32) == 32);
	CHECK(bf_find_next_set_bit(bf, 33) == 70);
	CHECK(bf_find_next_set_bit(bf, 71) == WORD_MAX);

	bf_for_each_set_bit(bf, b, 96) {
		if (n < 4)
			seen[n] = b;
		n++;
	}
	CHECK(n == 4);
	if (n == 4) {
		CHECK(seen[0] == 3);
		CHECK(seen[1] == 31);
		CHECK(seen[2] == 32);
		CHECK(seen[3] == 70);
	}

	bf_free(bf);
}

static void test_find_next_clear_bit_wrap(void)
{
	bitfield_t bf;

	bf_init(bf, 63);
	CHECK(bf.m == 2);

	/* empty: first clear bit at or after start_index */
	CHECK(bf_find_next_clear_bit_wrap(&bf, 0, 63) == 0);
	CHECK(bf_find_next_clear_bit_wrap(&bf, 10, 63) == 10);
	CHECK(bf_find_next_clear_bit_wrap(&bf, 40, 63) == 40);

	/* start_index beyond max_index restarts from 0 */
	CHECK(bf_find_next_clear_bit_wrap(&bf, 100, 63) == 0);

	/* occupied bits are skipped */
	bf.data[0] |= 0x7;
	CHECK(bf_find_next_clear_bit_wrap(&bf, 0, 63) == 3);
	CHECK(bf_find_next_clear_bit_wrap(&bf, 1, 63) == 3);

	/* a full first word is skipped entirely */
	bf.data[0] = WORD_MAX;
	CHECK(bf_find_next_clear_bit_wrap(&bf, 0, 63) == WORD_SIZE);

	/* wraps around to a hole before start_index when the tail is full */
	bf.data[0] = WORD_MAX & ~(1U << 5);
	bf.data[1] = WORD_MAX;
	CHECK(bf_find_next_clear_bit_wrap(&bf, 10, 63) == 5);

	/* nothing free anywhere */
	bf.data[0] = WORD_MAX;
	CHECK(bf_find_next_clear_bit_wrap(&bf, 10, 63) == WORD_MAX);

	bf_free(bf);
}

int main(int argc, char **argv)
{
	test_assign_release();
	test_zero_index();
	test_cmp();
	test_set_bit_growth();
	test_find_next_set_bit();
	test_find_next_clear_bit_wrap();

	if (!fail)
		printf("OK\n");
	return fail;
}
