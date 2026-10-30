// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * lib/mlag.c string helper and stream decoder unit tests
 */
#include <zebra.h>

#include "stream.h"
#include "mlag.h"

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

static void test_role2str(void)
{
	char buf[MLAG_ROLE_STRSIZE];

	CHECK(!strcmp(mlag_role2str(MLAG_ROLE_NONE, buf, sizeof(buf)), "NONE"),
	      "role none: '%s'", buf);
	CHECK(!strcmp(mlag_role2str(MLAG_ROLE_PRIMARY, buf, sizeof(buf)),
		      "PRIMARY"),
	      "role primary: '%s'", buf);
	CHECK(!strcmp(mlag_role2str(MLAG_ROLE_SECONDARY, buf, sizeof(buf)),
		      "SECONDARY"),
	      "role secondary: '%s'", buf);

	/* returns the caller's buffer, truncating safely */
	char small[4];
	char *r = mlag_role2str(MLAG_ROLE_PRIMARY, small, sizeof(small));

	CHECK(r == small, "role: returned buffer differs");
	CHECK(!strcmp(small, "PRI"), "role trunc: '%s'", small);
}

static void test_msgid2str(void)
{
	static const struct {
		enum mlag_msg_type type;
		const char *str;
	} cases[] = {
		{MLAG_REGISTER, "Register"},
		{MLAG_DEREGISTER, "De-Register"},
		{MLAG_MROUTE_ADD, "Mroute add"},
		{MLAG_MROUTE_DEL, "Mroute del"},
		{MLAG_DUMP, "Mlag Replay"},
		{MLAG_MROUTE_ADD_BULK, "Mroute Add Batch"},
		{MLAG_MROUTE_DEL_BULK, "Mroute Del Batch"},
		{MLAG_STATUS_UPDATE, "Mlag Status"},
		{MLAG_VXLAN_UPDATE, "Mlag vxlan update"},
		{MLAG_PEER_FRR_STATUS, "Mlag Peer FRR Status"},
		{MLAG_PIM_CFG_DUMP, "Mlag Pim Configuration Dump"},
		{MLAG_MSG_NONE, "Unknown 0"},
	};
	char buf[64];

	for (size_t i = 0; i < array_size(cases); i++) {
		char *r = mlag_lib_msgid_to_str(cases[i].type, buf,
						sizeof(buf));

		CHECK(r == buf, "msgid %d: returned buffer differs",
		      cases[i].type);
		CHECK(!strcmp(buf, cases[i].str), "msgid %d: '%s' != '%s'",
		      cases[i].type, buf, cases[i].str);
	}
}

static void test_decode_hdr(void)
{
	struct stream *s = stream_new(64);
	struct mlag_msg msg;
	size_t len = 0;

	/* NULL arguments */
	CHECK(mlag_lib_decode_mlag_hdr(NULL, &msg, &len) == -1, "hdr: NULL s");
	CHECK(mlag_lib_decode_mlag_hdr(s, NULL, &len) == -1, "hdr: NULL msg");

	/* shorter than the 8 byte header */
	stream_putl(s, MLAG_REGISTER);
	stream_putw(s, 3);
	CHECK(mlag_lib_decode_mlag_hdr(s, &msg, &len) == -1, "hdr: short");

	/* header plus 5 bytes of payload */
	stream_putw(s, 9);
	stream_put(s, "hello", 5);
	stream_set_getp(s, 0);
	CHECK(mlag_lib_decode_mlag_hdr(s, &msg, &len) == 0, "hdr: valid");
	CHECK(len == 5, "hdr: payload len %zu", len);
	CHECK(msg.msg_type == MLAG_REGISTER, "hdr: type %d", msg.msg_type);
	CHECK(msg.data_len == 3, "hdr: data_len %u", msg.data_len);
	CHECK(msg.msg_cnt == 9, "hdr: msg_cnt %u", msg.msg_cnt);

	stream_free(s);
}

static void test_decode_status(void)
{
	struct stream *s = stream_new(64);
	struct mlag_status st;
	char rif[IFNAMSIZ] = "peerlink.4094";

	CHECK(mlag_lib_decode_mlag_status(NULL, &st) == -1, "status: NULL s");
	CHECK(mlag_lib_decode_mlag_status(s, NULL) == -1, "status: NULL msg");

	/* truncated stream fails through stream_failure */
	stream_put(s, rif, IFNAMSIZ);
	stream_set_getp(s, 0);
	CHECK(mlag_lib_decode_mlag_status(s, &st) == -1, "status: truncated");

	stream_reset(s);
	stream_put(s, rif, IFNAMSIZ);
	stream_putl(s, MLAG_ROLE_SECONDARY);
	stream_putl(s, MLAG_STATE_RUNNING);
	stream_set_getp(s, 0);
	CHECK(mlag_lib_decode_mlag_status(s, &st) == 0, "status: valid");
	CHECK(!strcmp(st.peerlink_rif, rif), "status: rif '%s'",
	      st.peerlink_rif);
	CHECK(st.my_role == MLAG_ROLE_SECONDARY, "status: role %d",
	      st.my_role);
	CHECK(st.peer_state == MLAG_STATE_RUNNING, "status: state %d",
	      st.peer_state);

	stream_free(s);
}

static void test_decode_vxlan_frr(void)
{
	struct stream *s = stream_new(64);
	struct mlag_vxlan vx;
	struct mlag_frr_status fs;

	CHECK(mlag_lib_decode_vxlan_update(NULL, &vx) == -1, "vxlan: NULL s");
	CHECK(mlag_lib_decode_vxlan_update(s, NULL) == -1, "vxlan: NULL msg");
	CHECK(mlag_lib_decode_vxlan_update(s, &vx) == -1, "vxlan: empty");

	stream_putl(s, 0x0a000001);
	stream_putl(s, 0x0a000002);
	stream_set_getp(s, 0);
	CHECK(mlag_lib_decode_vxlan_update(s, &vx) == 0, "vxlan: valid");
	CHECK(vx.anycast_ip == 0x0a000001, "vxlan: anycast %x", vx.anycast_ip);
	CHECK(vx.local_ip == 0x0a000002, "vxlan: local %x", vx.local_ip);

	stream_reset(s);
	CHECK(mlag_lib_decode_frr_status(NULL, &fs) == -1, "frr: NULL s");
	CHECK(mlag_lib_decode_frr_status(s, NULL) == -1, "frr: NULL msg");
	CHECK(mlag_lib_decode_frr_status(s, &fs) == -1, "frr: empty");

	stream_putl(s, MLAG_FRR_STATE_UP);
	stream_set_getp(s, 0);
	CHECK(mlag_lib_decode_frr_status(s, &fs) == 0, "frr: valid");
	CHECK(fs.frr_state == MLAG_FRR_STATE_UP, "frr: state %d", fs.frr_state);

	stream_free(s);
}

static void test_decode_mroute(void)
{
	struct stream *s = stream_new(256);
	struct mlag_mroute_add add;
	struct mlag_mroute_del del;
	char vrf[VRF_NAMSIZ] = "red";
	char ifn[IFNAMSIZ] = "vlan100";
	size_t len;

	/* add: NULL and too-short length are rejected before reading */
	len = 1000;
	CHECK(mlag_lib_decode_mroute_add(NULL, &add, &len) == -1,
	      "add: NULL s");
	CHECK(mlag_lib_decode_mroute_add(s, NULL, &len) == -1, "add: NULL msg");
	len = 0;
	CHECK(mlag_lib_decode_mroute_add(s, &add, &len) == -1, "add: len 0");

	/* add: length ok but stream empty -> stream_failure */
	len = 1000;
	CHECK(mlag_lib_decode_mroute_add(s, &add, &len) == -1,
	      "add: empty stream");

	stream_put(s, vrf, VRF_NAMSIZ);
	stream_putl(s, 0x01020304);
	stream_putl(s, 0xe0000001);
	stream_putl(s, 77);
	stream_putl(s, MLAG_OWNER_VXLAN);
	stream_putc(s, 1);
	stream_putc(s, 0);
	stream_putl(s, 5);
	stream_put(s, ifn, IFNAMSIZ);
	stream_set_getp(s, 0);
	len = 1000;
	CHECK(mlag_lib_decode_mroute_add(s, &add, &len) == 0, "add: valid");
	CHECK(!strcmp(add.vrf_name, "red"), "add: vrf '%s'", add.vrf_name);
	CHECK(add.source_ip == 0x01020304, "add: src %x", add.source_ip);
	CHECK(add.group_ip == 0xe0000001, "add: grp %x", add.group_ip);
	CHECK(add.cost_to_rp == 77, "add: cost %u", add.cost_to_rp);
	CHECK(add.owner_id == MLAG_OWNER_VXLAN, "add: owner %d", add.owner_id);
	CHECK(add.am_i_dr && !add.am_i_dual_active, "add: flags");
	CHECK(add.vrf_id == 5, "add: vrf_id %u", add.vrf_id);
	CHECK(!strcmp(add.intf_name, "vlan100"), "add: intf '%s'",
	      add.intf_name);

	/* del */
	stream_reset(s);
	len = 0;
	CHECK(mlag_lib_decode_mroute_del(s, &del, &len) == -1, "del: len 0");
	CHECK(mlag_lib_decode_mroute_del(NULL, &del, &len) == -1,
	      "del: NULL s");
	len = 1000;
	CHECK(mlag_lib_decode_mroute_del(s, &del, &len) == -1,
	      "del: empty stream");

	stream_put(s, vrf, VRF_NAMSIZ);
	stream_putl(s, 0x01020304);
	stream_putl(s, 0xe0000001);
	stream_putl(s, MLAG_OWNER_INTERFACE);
	stream_putl(s, 5);
	stream_put(s, ifn, IFNAMSIZ);
	stream_set_getp(s, 0);
	len = 1000;
	CHECK(mlag_lib_decode_mroute_del(s, &del, &len) == 0, "del: valid");
	CHECK(!strcmp(del.vrf_name, "red"), "del: vrf '%s'", del.vrf_name);
	CHECK(del.source_ip == 0x01020304 && del.group_ip == 0xe0000001,
	      "del: addrs");
	CHECK(del.owner_id == MLAG_OWNER_INTERFACE, "del: owner %d",
	      del.owner_id);
	CHECK(del.vrf_id == 5, "del: vrf_id %u", del.vrf_id);
	CHECK(!strcmp(del.intf_name, "vlan100"), "del: intf '%s'",
	      del.intf_name);

	stream_free(s);
}

int main(int argc, char **argv)
{
	test_role2str();
	test_msgid2str();
	test_decode_hdr();
	test_decode_status();
	test_decode_vxlan_frr();
	test_decode_mroute();

	printf("%s\n", fail ? "FAILED" : "all mlag tests passed");
	return fail;
}
