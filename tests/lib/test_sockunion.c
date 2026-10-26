// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * sockunion helper tests (lib/sockunion.c)
 */
#include <zebra.h>

#include "sockunion.h"
#include "memory.h"

static int fail;

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL line %d: %s\n", __LINE__, #cond);         \
			fail = 1;                                              \
		}                                                              \
	} while (0)

static void test_parse_format(void)
{
	union sockunion su;
	char buf[SU_ADDRSTRLEN];

	CHECK(str2sockunion("192.0.2.1", &su) == 0);
	CHECK(sockunion_family(&su) == AF_INET);
	CHECK(!strcmp(sockunion2str(&su, buf, sizeof(buf)), "192.0.2.1"));
	CHECK(!strcmp(inet_sutop(&su, buf), "192.0.2.1"));
	CHECK(sockunion_get_addrlen(&su) == 4);

	CHECK(str2sockunion("2001:db8::1", &su) == 0);
	CHECK(sockunion_family(&su) == AF_INET6);
	CHECK(!strcmp(sockunion2str(&su, buf, sizeof(buf)), "2001:db8::1"));
	CHECK(!strcmp(inet_sutop(&su, buf), "2001:db8::1"));
	CHECK(sockunion_get_addrlen(&su) == 16);

	CHECK(str2sockunion(NULL, &su) == -1);
	CHECK(str2sockunion("not-an-address", &su) == -1);
	CHECK(str2sockunion("192.0.2.256", &su) == -1);
	CHECK(str2sockunion("", &su) == -1);

	memset(&su, 0, sizeof(su));
	CHECK(!strcmp(sockunion2str(&su, buf, sizeof(buf)), "(unspec)"));
	CHECK(sockunion_get_addr(&su) == NULL);
	CHECK(sockunion_get_addrlen(&su) == 0);

	su.sa.sa_family = AF_UNIX;
	CHECK(!strcmp(sockunion2str(&su, buf, sizeof(buf)), "(af 1)"));
}

static void test_str2su_dup(void)
{
	union sockunion *su, *dup;

	CHECK(sockunion_str2su("bogus") == NULL);

	su = sockunion_str2su("198.51.100.7");
	CHECK(su != NULL);
	if (!su)
		return;
	CHECK(sockunion_family(su) == AF_INET);

	dup = sockunion_dup(su);
	CHECK(dup != NULL && dup != su);
	if (dup) {
		CHECK(sockunion_same(su, dup));
		CHECK(sockunion_cmp(su, dup) == 0);
		sockunion_free(dup);
	}
	sockunion_free(su);
}

static void test_cmp_same(void)
{
	union sockunion a, b, c6, d6;

	str2sockunion("10.0.0.1", &a);
	str2sockunion("10.0.0.2", &b);

	CHECK(sockunion_cmp(&a, &a) == 0);
	CHECK(sockunion_cmp(&a, &b) == -1);
	CHECK(sockunion_cmp(&b, &a) == 1);
	CHECK(sockunion_same(&a, &a));
	CHECK(!sockunion_same(&a, &b));

	str2sockunion("2001:db8::1", &c6);
	str2sockunion("2001:db8::2", &d6);
	CHECK(sockunion_cmp(&c6, &c6) == 0);
	CHECK(sockunion_cmp(&c6, &d6) < 0);
	CHECK(sockunion_cmp(&d6, &c6) > 0);
	CHECK(sockunion_same(&c6, &c6));
	CHECK(!sockunion_same(&c6, &d6));

	/* different families never match; AF_INET < AF_INET6 */
	CHECK(!sockunion_same(&a, &c6));
	CHECK(sockunion_cmp(&a, &c6) == -1);
	CHECK(sockunion_cmp(&c6, &a) == 1);

	/* scope id is a tiebreaker for cmp */
	d6 = c6;
	d6.sin6.sin6_scope_id = 5;
	CHECK(sockunion_cmp(&c6, &d6) == -1);
	CHECK(sockunion_cmp(&d6, &c6) == 1);

	/* scope id only matters for same() on link-local when both set */
	str2sockunion("fe80::1", &c6);
	d6 = c6;
	c6.sin6.sin6_scope_id = 2;
	d6.sin6.sin6_scope_id = 3;
	CHECK(!sockunion_same(&c6, &d6));
	d6.sin6.sin6_scope_id = 2;
	CHECK(sockunion_same(&c6, &d6));
	d6.sin6.sin6_scope_id = 0;
	CHECK(sockunion_same(&c6, &d6));
}

static void test_hash_null_set(void)
{
	union sockunion a, b, c, set;
	const uint8_t v4[4] = {203, 0, 113, 9};
	const uint8_t v6[16] = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0,
				0,    0,    0,    0,    0, 0, 0, 1};
	char buf[SU_ADDRSTRLEN];

	str2sockunion("10.1.2.3", &a);
	str2sockunion("10.1.2.3", &b);
	str2sockunion("10.1.2.4", &c);
	CHECK(sockunion_hash(&a) == sockunion_hash(&b));
	CHECK(sockunion_hash(&a) != sockunion_hash(&c));

	str2sockunion("2001:db8::1", &a);
	str2sockunion("2001:db8::1", &b);
	CHECK(sockunion_hash(&a) == sockunion_hash(&b));

	memset(&a, 0, sizeof(a));
	CHECK(sockunion_hash(&a) == 0);

	CHECK(sockunion_is_null(&a));
	str2sockunion("0.0.0.0", &a);
	CHECK(sockunion_is_null(&a));
	str2sockunion("::", &a);
	CHECK(sockunion_is_null(&a));
	str2sockunion("0.0.0.1", &a);
	CHECK(!sockunion_is_null(&a));
	str2sockunion("::1", &a);
	CHECK(!sockunion_is_null(&a));

	CHECK(family2addrsize(AF_INET) == 4);
	CHECK(family2addrsize(AF_INET6) == 16);
	CHECK(family2addrsize(AF_UNIX) == 0);

	memset(&set, 0, sizeof(set));
	sockunion_set(&set, AF_INET, v4, sizeof(v4));
	CHECK(!strcmp(sockunion2str(&set, buf, sizeof(buf)), "203.0.113.9"));
	CHECK(!memcmp(sockunion_get_addr(&set), v4, sizeof(v4)));

	memset(&set, 0, sizeof(set));
	sockunion_set(&set, AF_INET6, v6, sizeof(v6));
	CHECK(!strcmp(sockunion2str(&set, buf, sizeof(buf)), "2001:db8::1"));

	/* length mismatch leaves the target untouched */
	memset(&set, 0, sizeof(set));
	sockunion_set(&set, AF_INET, v6, sizeof(v6));
	CHECK(sockunion_family(&set) == AF_UNSPEC);
}

int main(int argc, char **argv)
{
	test_parse_format();
	test_str2su_dup();
	test_cmp_same();
	test_hash_null_set();

	if (!fail)
		printf("all sockunion tests passed\n");
	return fail;
}
