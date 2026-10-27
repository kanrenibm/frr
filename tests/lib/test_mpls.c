// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * mpls_str2label / mpls_label2str unit tests
 */
#include <zebra.h>

#include "mpls.h"

static int fail;

#define CHECK(cond, ...)                                                       \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL line %d: ", __LINE__);                    \
			printf(__VA_ARGS__);                                   \
			printf("\n");                                          \
			fail = 1;                                              \
		}                                                              \
	} while (0)

static void check_parse(const char *str, int expect_rc, uint8_t expect_num,
			const mpls_label_t *expect_labels)
{
	uint8_t num = 0xff;
	mpls_label_t labels[MPLS_MAX_LABELS];
	int rc;

	memset(labels, 0, sizeof(labels));
	rc = mpls_str2label(str, &num, labels);
	CHECK(rc == expect_rc, "\"%s\": rc %d, expected %d", str, rc,
	      expect_rc);
	CHECK(num == expect_num, "\"%s\": num_labels %u, expected %u", str, num,
	      expect_num);
	if (rc == 0 && num == expect_num)
		CHECK(!memcmp(labels, expect_labels,
			      expect_num * sizeof(mpls_label_t)),
		      "\"%s\": label values differ", str);
}

static void check_fmt(uint8_t num, const mpls_label_t *labels, const char *exp)
{
	char buf[256];
	char *ret = mpls_label2str(num, labels, buf, sizeof(buf),
				   ZEBRA_LSP_STATIC, 0);

	CHECK(ret == buf, "mpls_label2str must return buf");
	CHECK(!strcmp(buf, exp), "label2str: \"%s\", expected \"%s\"", buf,
	      exp);
}

int main(int argc, char **argv)
{
	static const mpls_label_t one[] = {16};
	static const mpls_label_t two[] = {100, 200};
	static const mpls_label_t edge[] = {16, MPLS_LABEL_UNRESERVED_MAX};
	mpls_label_t many[MPLS_MAX_LABELS];
	char big[256], *p = big;
	int i;

	/* successful parses */
	check_parse("16", 0, 1, one);
	check_parse("100/200", 0, 2, two);
	check_parse("16/1048575", 0, 2, edge);

	/* maximum number of labels is accepted, one more is not */
	for (i = 0; i < MPLS_MAX_LABELS; i++) {
		many[i] = 16 + i;
		p += snprintf(p, sizeof(big) - (p - big), "%s%u", i ? "/" : "",
			      many[i]);
	}
	check_parse(big, 0, MPLS_MAX_LABELS, many);
	snprintf(p, sizeof(big) - (p - big), "/99");
	check_parse(big, -3, 0, NULL);

	/* format errors */
	check_parse("abc", -1, 0, NULL);
	check_parse("16/abc", -1, 0, NULL);
	check_parse("16x", -1, 0, NULL);

	/* reserved or out of range labels */
	check_parse("0", -2, 0, NULL);
	check_parse("15", -2, 0, NULL);
	check_parse("100/3", -2, 0, NULL);
	check_parse("1048576", -2, 0, NULL);
	check_parse("", -2, 0, NULL);
	check_parse("16/", -2, 0, NULL);

	/* a failed parse leaves the caller's label array untouched */
	{
		uint8_t num;
		mpls_label_t labels[MPLS_MAX_LABELS];

		memset(labels, 0xa5, sizeof(labels));
		CHECK(mpls_str2label("100/abc", &num, labels) == -1,
		      "expected parse failure");
		for (i = 0; i < MPLS_MAX_LABELS; i++)
			CHECK(labels[i] == 0xa5a5a5a5,
			      "labels[%d] modified on failure", i);
	}

	/* formatting (non-pretty) */
	check_fmt(0, NULL, "");
	check_fmt(1, one, "16");
	check_fmt(2, two, "100/200");
	check_fmt(2, edge, "16/1048575");

	/* output is truncated, not overflowed, on a small buffer */
	{
		char small[6];

		mpls_label2str(2, two, small, sizeof(small), ZEBRA_LSP_STATIC,
			       0);
		CHECK(!strcmp(small, "100/2"), "truncated: \"%s\"", small);
	}

	if (fail)
		printf("FAILED\n");
	return fail;
}
