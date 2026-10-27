// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * lib/iso.c printfrr extension tests (%pSY, %pPN, %pLS, %pIS)
 */
#include <zebra.h>

#include "iso.h"
#include "printfrr.h"

static int fail;

static void check(const char *got, const char *expect)
{
	if (strcmp(got, expect)) {
		printf("FAIL: got \"%s\", expected \"%s\"\n", got, expect);
		fail = 1;
	}
}

int main(int argc, char **argv)
{
	char buf[ISO_ADDR_STRLEN + 8];
	const uint8_t id[8] = {0x00, 0x01, 0xab, 0xcd, 0xef, 0x12, 0x03, 0x04};
	struct iso_address ia = {};
	uint8_t *null_id = NULL;
	struct iso_address *null_ia = NULL;

	snprintfrr(buf, sizeof(buf), "%pSY", id);
	check(buf, "0001.abcd.ef12");

	snprintfrr(buf, sizeof(buf), "%pPN", id);
	check(buf, "0001.abcd.ef12.03");

	snprintfrr(buf, sizeof(buf), "%pLS", id);
	check(buf, "0001.abcd.ef12.03-04");

	snprintfrr(buf, sizeof(buf), "%pSY", null_id);
	check(buf, "(null)");
	snprintfrr(buf, sizeof(buf), "%pPN", null_id);
	check(buf, "(null)");
	snprintfrr(buf, sizeof(buf), "%pLS", null_id);
	check(buf, "(null)");
	snprintfrr(buf, sizeof(buf), "%pIS", null_ia);
	check(buf, "(null)");

	/* area address 49.0001 followed by system ID and NSEL */
	ia.addr_len = 3;
	ia.area_addr[0] = 0x49;
	ia.area_addr[1] = 0x00;
	ia.area_addr[2] = 0x01;
	memcpy(&ia.area_addr[3], "\x0a\x0b\x0c\x0d\x0e\x0f\x00", 7);

	snprintfrr(buf, sizeof(buf), "%pIS", &ia);
	check(buf, "49.0001");

	/* 'l' adds the 6 byte system ID and 1 byte NSEL */
	snprintfrr(buf, sizeof(buf), "%pISl", &ia);
	check(buf, "49.0001.0a0b.0c0d.0e0f.00");

	/* single byte area address */
	ia.addr_len = 1;
	snprintfrr(buf, sizeof(buf), "%pIS", &ia);
	check(buf, "49");

	if (!fail)
		printf("OK\n");
	return fail;
}
