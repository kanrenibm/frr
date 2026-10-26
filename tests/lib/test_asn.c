// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * ASN string parsing, notation detection, matching and formatting tests
 */
#include <zebra.h>

#include "asn.h"
#include "printfrr.h"

static int fail;

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL: line %d: %s\n", __LINE__, #cond);        \
			fail = 1;                                              \
		}                                                              \
	} while (0)

static void test_str2asn(void)
{
	as_t asn = 0;

	asn_relax_as_zero(false);

	CHECK(asn_str2asn("65500", &asn) && asn == 65500);
	CHECK(asn_str2asn("4294967295", &asn) && asn == 4294967295U);
	CHECK(asn_str2asn("1.1", &asn) && asn == 65537);
	CHECK(asn_str2asn("65535.65535", &asn) && asn == 4294967295U);
	CHECK(asn_str2asn("0.1", &asn) && asn == 1);

	/* NULL asn pointer only validates */
	CHECK(asn_str2asn("100", NULL));
	CHECK(asn_str2asn("1.1", NULL));

	/* invalid input */
	CHECK(!asn_str2asn(NULL, &asn));
	CHECK(!asn_str2asn("", &asn));
	CHECK(!asn_str2asn("abc", &asn));
	CHECK(!asn_str2asn("-1", &asn));
	CHECK(!asn_str2asn("4294967296", &asn));
	CHECK(!asn_str2asn("01", &asn));
	CHECK(!asn_str2asn("1.01", &asn));
	CHECK(!asn_str2asn("1.65536", &asn));
	CHECK(!asn_str2asn("65536.1", &asn));
	CHECK(!asn_str2asn("1.1x", &asn));

	/* AS 0 / 0.0 forbidden unless relaxed */
	CHECK(!asn_str2asn("0", &asn));
	CHECK(!asn_str2asn("0.0", &asn));
	asn_relax_as_zero(true);
	CHECK(asn_str2asn("0", &asn) && asn == 0);
	CHECK(asn_str2asn("0.0", &asn) && asn == 0);
	asn_relax_as_zero(false);
	CHECK(!asn_str2asn("0", &asn));
}

static void test_notation(void)
{
	as_t asn = 0;
	enum asnotation_mode mode;

	mode = ASNOTATION_UNDEFINED;
	CHECK(asn_str2asn_notation("100", &asn, &mode));
	CHECK(mode == ASNOTATION_PLAIN && asn == 100);

	CHECK(asn_str2asn_notation("1.5", &asn, &mode));
	CHECK(mode == ASNOTATION_DOT && asn == 65541);

	CHECK(asn_str2asn_notation("0.5", &asn, &mode));
	CHECK(mode == ASNOTATION_DOTPLUS && asn == 5);

	CHECK(!asn_str2asn_notation("junk", &asn, &mode));

	CHECK(strcmp(asn_mode2str(ASNOTATION_PLAIN), "plain") == 0);
	CHECK(strcmp(asn_mode2str(ASNOTATION_DOT), "dot") == 0);
	CHECK(strcmp(asn_mode2str(ASNOTATION_DOTPLUS), "dot+") == 0);
	CHECK(strcmp(asn_mode2str(ASNOTATION_UNDEFINED), "undefined") == 0);
	CHECK(strcmp(asn_mode2str((enum asnotation_mode)42),
		     "Unrecognized AS notation mode") == 0);
}

static void test_parse_and_match(void)
{
	as_t asn = 0;
	bool found = false;
	const char *s = "65000 rest";
	const char *next;

	next = asn_str2asn_parse(s, &asn, &found);
	CHECK(found && asn == 65000);
	CHECK(next == s + 5);

	found = true;
	next = asn_str2asn_parse("xyz", &asn, &found);
	CHECK(!found);
	CHECK(strcmp(next, "xyz") == 0);

	/* found_ptr may be NULL */
	next = asn_str2asn_parse("7", &asn, NULL);
	CHECK(asn == 7 && *next == '\0');

	CHECK(asn_str2asn_match("65000") == exact_match);
	CHECK(asn_str2asn_match("1.1") == exact_match);
	CHECK(asn_str2asn_match("1.") == partly_match);
	CHECK(asn_str2asn_match("0.0") == partly_match);
	CHECK(asn_str2asn_match("abc") == no_match);
	CHECK(asn_str2asn_match("0") == no_match);
}

static void test_format(void)
{
	char buf[64];
	as_t asn;

	CHECK(strcmp(asn_asn2asplain(65537), "65537") == 0);
	CHECK(strcmp(asn_asn2asplain(4294967295U), "4294967295") == 0);

	asn = 65537;
	asn_asn2string(&asn, buf, sizeof(buf), ASNOTATION_PLAIN);
	CHECK(strcmp(buf, "65537") == 0);
	asn_asn2string(&asn, buf, sizeof(buf), ASNOTATION_DOT);
	CHECK(strcmp(buf, "1.1") == 0);
	asn_asn2string(&asn, buf, sizeof(buf), ASNOTATION_DOTPLUS);
	CHECK(strcmp(buf, "1.1") == 0);

	/* small AS: dot stays plain, dot+ is always N.N */
	asn = 100;
	asn_asn2string(&asn, buf, sizeof(buf), ASNOTATION_DOT);
	CHECK(strcmp(buf, "100") == 0);
	asn_asn2string(&asn, buf, sizeof(buf), ASNOTATION_DOTPLUS);
	CHECK(strcmp(buf, "0.100") == 0);

	/* truncation is bounded by len */
	asn = 4294967295U;
	asn_asn2string(&asn, buf, 4, ASNOTATION_PLAIN);
	CHECK(strcmp(buf, "429") == 0);

	/* printfrr extensions */
	asn = 65537;
	snprintfrr(buf, sizeof(buf), "%pASP", &asn);
	CHECK(strcmp(buf, "65537") == 0);
	snprintfrr(buf, sizeof(buf), "%pASD", &asn);
	CHECK(strcmp(buf, "1.1") == 0);
	snprintfrr(buf, sizeof(buf), "%pASE", &asn);
	CHECK(strcmp(buf, "1.1") == 0);
	snprintfrr(buf, sizeof(buf), "%pASP", (as_t *)NULL);
	CHECK(strcmp(buf, "(null)") == 0);
	snprintfrr(buf, sizeof(buf), ASN_FORMAT(ASNOTATION_DOT), &asn);
	CHECK(strcmp(buf, "1.1") == 0);
}

int main(void)
{
	test_str2asn();
	test_notation();
	test_parse_and_match();
	test_format();

	if (fail)
		return 1;
	printf("OK\n");
	return 0;
}
