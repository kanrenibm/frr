// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * lib/monotime.h helper tests: interval formatting, date string, since/until
 */
#include <zebra.h>

#include "monotime.h"

static int fail;

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL: line %d: %s\n", __LINE__, #cond);        \
			fail = 1;                                              \
		}                                                              \
	} while (0)

static void test_interval(void)
{
	char buf[32];

	CHECK(!strcmp(frrtime_to_interval(0, buf, sizeof(buf)), "00:00:00"));
	CHECK(!strcmp(frrtime_to_interval(3661, buf, sizeof(buf)), "01:01:01"));
	CHECK(!strcmp(frrtime_to_interval(ONE_DAY_SECOND - 1, buf, sizeof(buf)),
		      "23:59:59"));
	CHECK(!strcmp(frrtime_to_interval(ONE_DAY_SECOND, buf, sizeof(buf)),
		      "1d00h00m"));
	CHECK(!strcmp(frrtime_to_interval(ONE_DAY_SECOND + 3600 + 120, buf,
					  sizeof(buf)),
		      "1d01h02m"));
	CHECK(!strcmp(frrtime_to_interval(ONE_WEEK_SECOND - 1, buf, sizeof(buf)),
		      "6d23h59m"));
	CHECK(!strcmp(frrtime_to_interval(ONE_WEEK_SECOND, buf, sizeof(buf)),
		      "01w0d00h"));
	CHECK(!strcmp(frrtime_to_interval(ONE_WEEK_SECOND + ONE_DAY_SECOND +
						  2 * 3600,
					  buf, sizeof(buf)),
		      "01w1d02h"));
}

static void test_date_string(void)
{
	char buf[32];
	struct timeval tv;
	time_t now = monotime(&tv);

	CHECK(!strcmp(time_to_date_string(0, buf, sizeof(buf)), "never"));

	/* an event that just happened is ~0 seconds old */
	time_to_date_string(now, buf, sizeof(buf));
	CHECK(!strncmp(buf, "0:00:00:0", 9) || !strncmp(buf, "0:00:00:1", 9));
}

static void test_time_strings(void)
{
	char buf[MONOTIME_STRLEN];
	time_t now = monotime(NULL);
	time_t real;

	time_to_string_json(now, buf);
	CHECK(strlen(buf) > 0);
	CHECK(buf[strlen(buf) - 1] != '\n');

	real = time_to_epoch(now);
	CHECK(llabs((long long)(time(NULL) - real)) <= 2);
}

static void test_since_until(void)
{
	struct timeval ref, out;
	int64_t since, until;

	monotime(&ref);
	ref.tv_sec -= 5;
	since = monotime_since(&ref, &out);
	CHECK(since >= 5000000 && since < 7000000);
	CHECK(out.tv_sec >= 5 && out.tv_sec < 7);

	monotime(&ref);
	ref.tv_sec += 5;
	until = monotime_until(&ref, &out);
	CHECK(until > 3000000 && until <= 5000000);
	CHECK(monotime_since(&ref, NULL) < 0);
}

int main(void)
{
	test_interval();
	test_date_string();
	test_time_strings();
	test_since_until();

	if (!fail)
		printf("monotime tests passed\n");
	return fail;
}
