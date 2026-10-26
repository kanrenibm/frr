// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * lib/srv6.c string conversion helpers unit test
 */
#include <zebra.h>

#include "srv6.h"

static void check_action(uint32_t action, const char *plain, const char *csid)
{
	assert(!strcmp(seg6local_action2str(action), plain));
	assert(!strcmp(seg6local_action2str_with_next_csid(action, false), plain));
	assert(!strcmp(seg6local_action2str_with_next_csid(action, true), csid));
}

static void test_action2str(void)
{
	check_action(ZEBRA_SEG6_LOCAL_ACTION_END, "End", "uN");
	check_action(ZEBRA_SEG6_LOCAL_ACTION_END_X, "End.X", "uA");
	check_action(ZEBRA_SEG6_LOCAL_ACTION_END_T, "End.T", "uDT");
	check_action(ZEBRA_SEG6_LOCAL_ACTION_END_DX2, "End.DX2", "uDX2");
	check_action(ZEBRA_SEG6_LOCAL_ACTION_END_DX6, "End.DX6", "uDX6");
	check_action(ZEBRA_SEG6_LOCAL_ACTION_END_DX4, "End.DX4", "uDX4");
	check_action(ZEBRA_SEG6_LOCAL_ACTION_END_DT6, "End.DT6", "uDT6");
	check_action(ZEBRA_SEG6_LOCAL_ACTION_END_DT4, "End.DT4", "uDT4");
	check_action(ZEBRA_SEG6_LOCAL_ACTION_END_B6, "End.B6", "uB6");
	check_action(ZEBRA_SEG6_LOCAL_ACTION_END_B6_ENCAP, "End.B6.Encap",
		     "uB6.Encap");
	check_action(ZEBRA_SEG6_LOCAL_ACTION_END_BM, "End.BM", "uBM");
	check_action(ZEBRA_SEG6_LOCAL_ACTION_END_DT46, "End.DT46", "uDT46");

	/* behaviors without a uSID variant ignore the flag */
	check_action(ZEBRA_SEG6_LOCAL_ACTION_END_S, "End.S", "End.S");
	check_action(ZEBRA_SEG6_LOCAL_ACTION_END_AS, "End.AS", "End.AS");
	check_action(ZEBRA_SEG6_LOCAL_ACTION_END_AM, "End.AM", "End.AM");

	check_action(ZEBRA_SEG6_LOCAL_ACTION_UNSPEC, "unspec", "unspec");
	check_action(ZEBRA_SEG6_LOCAL_ACTION_END_BPF, "unknown", "unknown");
	check_action(0xffff, "unknown", "unknown");
}

static void test_segs(void)
{
	struct seg6_segs segs;
	char buf[SRV6_SEG_STRLEN];
	char small[16];
	int len;

	memset(&segs, 0, sizeof(segs));

	segs.num_segs = 0;
	memset(buf, 'x', sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';
	len = snprintf_seg6_segs(buf, sizeof(buf), &segs);
	assert(len == 0);
	assert(buf[0] == '\0');

	assert(inet_pton(AF_INET6, "2001:db8::1", &segs.segs[0]) == 1);
	segs.num_segs = 1;
	len = snprintf_seg6_segs(buf, sizeof(buf), &segs);
	assert(!strcmp(buf, "2001:db8::1"));
	assert(len == (int)strlen("2001:db8::1"));

	assert(inet_pton(AF_INET6, "2001:db8:1::2", &segs.segs[1]) == 1);
	assert(inet_pton(AF_INET6, "::", &segs.segs[2]) == 1);
	segs.num_segs = 3;
	len = snprintf_seg6_segs(buf, sizeof(buf), &segs);
	assert(!strcmp(buf, "2001:db8::1,2001:db8:1::2,::"));
	assert(len == (int)strlen(buf));

	/* output is truncated to the buffer and stays NUL terminated */
	len = snprintf_seg6_segs(small, sizeof(small), &segs);
	assert(len == (int)strlen(small));
	assert(len < (int)sizeof(small));
	assert(!strncmp(small, "2001:db8::1,2001", len));
}

static void test_context2str(void)
{
	struct seg6local_context ctx;
	char buf[128];
	const char *rv;

	memset(&ctx, 0, sizeof(ctx));

	/* End without flavors: empty string */
	rv = seg6local_context2str(buf, sizeof(buf), &ctx,
				   ZEBRA_SEG6_LOCAL_ACTION_END);
	assert(rv == buf);
	assert(!strcmp(buf, ""));

	/* End with each flavor combination */
	SET_SRV6_FLV_OP(ctx.flv.flv_ops, ZEBRA_SEG6_LOCAL_FLV_OP_PSP);
	seg6local_context2str(buf, sizeof(buf), &ctx,
			      ZEBRA_SEG6_LOCAL_ACTION_END);
	assert(!strcmp(buf, "(PSP)"));

	RESET_SRV6_FLV_OP(ctx.flv.flv_ops);
	SET_SRV6_FLV_OP(ctx.flv.flv_ops, ZEBRA_SEG6_LOCAL_FLV_OP_USP);
	seg6local_context2str(buf, sizeof(buf), &ctx,
			      ZEBRA_SEG6_LOCAL_ACTION_END);
	assert(!strcmp(buf, "(USP)"));

	RESET_SRV6_FLV_OP(ctx.flv.flv_ops);
	SET_SRV6_FLV_OP(ctx.flv.flv_ops, ZEBRA_SEG6_LOCAL_FLV_OP_USD);
	seg6local_context2str(buf, sizeof(buf), &ctx,
			      ZEBRA_SEG6_LOCAL_ACTION_END);
	assert(!strcmp(buf, "(USD)"));

	SET_SRV6_FLV_OP(ctx.flv.flv_ops, ZEBRA_SEG6_LOCAL_FLV_OP_PSP);
	SET_SRV6_FLV_OP(ctx.flv.flv_ops, ZEBRA_SEG6_LOCAL_FLV_OP_USP);
	seg6local_context2str(buf, sizeof(buf), &ctx,
			      ZEBRA_SEG6_LOCAL_ACTION_END);
	assert(!strcmp(buf, "(PSP/USP/USD)"));

	/* NEXT-C-SID alone is not rendered as a flavor */
	RESET_SRV6_FLV_OP(ctx.flv.flv_ops);
	SET_SRV6_FLV_OP(ctx.flv.flv_ops, ZEBRA_SEG6_LOCAL_FLV_OP_NEXT_CSID);
	seg6local_context2str(buf, sizeof(buf), &ctx,
			      ZEBRA_SEG6_LOCAL_ACTION_END);
	assert(!strcmp(buf, ""));

	/* per-behavior arguments */
	RESET_SRV6_FLV_OP(ctx.flv.flv_ops);
	assert(inet_pton(AF_INET6, "2001:db8::5", &ctx.nh6) == 1);
	assert(inet_pton(AF_INET, "192.0.2.7", &ctx.nh4) == 1);
	ctx.table = 100;

	seg6local_context2str(buf, sizeof(buf), &ctx,
			      ZEBRA_SEG6_LOCAL_ACTION_END_DX6);
	assert(!strcmp(buf, "nh6 2001:db8::5"));

	seg6local_context2str(buf, sizeof(buf), &ctx,
			      ZEBRA_SEG6_LOCAL_ACTION_END_DX4);
	assert(!strcmp(buf, "nh4 192.0.2.7"));

	seg6local_context2str(buf, sizeof(buf), &ctx,
			      ZEBRA_SEG6_LOCAL_ACTION_END_B6);
	assert(!strcmp(buf, "nh6 2001:db8::5"));
	seg6local_context2str(buf, sizeof(buf), &ctx,
			      ZEBRA_SEG6_LOCAL_ACTION_END_B6_ENCAP);
	assert(!strcmp(buf, "nh6 2001:db8::5"));

	seg6local_context2str(buf, sizeof(buf), &ctx,
			      ZEBRA_SEG6_LOCAL_ACTION_END_T);
	assert(!strcmp(buf, "table 100"));
	seg6local_context2str(buf, sizeof(buf), &ctx,
			      ZEBRA_SEG6_LOCAL_ACTION_END_DT6);
	assert(!strcmp(buf, "table 100"));
	seg6local_context2str(buf, sizeof(buf), &ctx,
			      ZEBRA_SEG6_LOCAL_ACTION_END_DT4);
	assert(!strcmp(buf, "table 100"));
	seg6local_context2str(buf, sizeof(buf), &ctx,
			      ZEBRA_SEG6_LOCAL_ACTION_END_DT46);
	assert(!strcmp(buf, "table 100"));

	/* flavors are appended to the argument */
	SET_SRV6_FLV_OP(ctx.flv.flv_ops, ZEBRA_SEG6_LOCAL_FLV_OP_PSP);
	seg6local_context2str(buf, sizeof(buf), &ctx,
			      ZEBRA_SEG6_LOCAL_ACTION_END_DT6);
	assert(!strcmp(buf, "table 100(PSP)"));

	/* behaviors with no printable context */
	seg6local_context2str(buf, sizeof(buf), &ctx,
			      ZEBRA_SEG6_LOCAL_ACTION_UNSPEC);
	assert(!strncmp(buf, "unknown(", 8));
	seg6local_context2str(buf, sizeof(buf), &ctx,
			      ZEBRA_SEG6_LOCAL_ACTION_END_BM);
	assert(!strncmp(buf, "unknown(", 8));
}

int main(int argc, char **argv)
{
	test_action2str();
	test_segs();
	test_context2str();
	return 0;
}
