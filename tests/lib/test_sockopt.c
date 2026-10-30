// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * lib/sockopt.c socket option helper unit tests
 */
#include <zebra.h>

#include <sys/socket.h>
#include <netinet/tcp.h>

#include "sockopt.h"

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

static int get_int(int sock, int level, int opt)
{
	int val = -1;
	socklen_t len = sizeof(val);

	if (getsockopt(sock, level, opt, &val, &len) < 0)
		return -1;
	return val;
}

static void test_buffers(void)
{
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	int ret;

	CHECK(s >= 0, "socket");

	ret = setsockopt_so_recvbuf(s, 32768);
	CHECK(ret > 0 && ret <= 32768, "recvbuf returned %d", ret);
	CHECK(getsockopt_so_recvbuf(s) > 0, "get recvbuf");

	ret = setsockopt_so_sendbuf(s, 32768);
	CHECK(ret > 0 && ret <= 32768, "sendbuf returned %d", ret);
	CHECK(getsockopt_so_sendbuf(s) > 0, "get sendbuf");

	/* invalid fd: no halving loop can succeed, ends at 0 */
	CHECK(setsockopt_so_recvbuf(-1, 1024) == 0, "bad fd recvbuf");
	CHECK(setsockopt_so_sendbuf(-1, 1024) == 0, "bad fd sendbuf");
	CHECK(getsockopt_so_recvbuf(-1) < 0, "bad fd get recvbuf");
	CHECK(getsockopt_so_sendbuf(-1) < 0, "bad fd get sendbuf");

	close(s);
}

static void test_ip_options(void)
{
	int s = socket(AF_INET, SOCK_DGRAM, 0);

	CHECK(setsockopt_ipv4_tos(s, 0x10) == 0, "tos");
	CHECK(get_int(s, IPPROTO_IP, IP_TOS) == 0x10, "tos readback");
	CHECK(setsockopt_ipv4_tos(-1, 0x10) < 0, "tos bad fd");

	CHECK(sockopt_ttl(AF_INET, s, 77) == 0, "ttl");
	CHECK(get_int(s, IPPROTO_IP, IP_TTL) == 77, "ttl readback");
	CHECK(sockopt_ttl(AF_INET, -1, 77) == -1, "ttl bad fd");
	/* unknown family is a no-op */
	CHECK(sockopt_ttl(AF_UNSPEC, s, 5) == 0, "ttl other family");
	CHECK(get_int(s, IPPROTO_IP, IP_TTL) == 77, "ttl unchanged");

	CHECK(setsockopt_ipv4_multicast_loop(s, 0) == 0, "mc loop off");
	CHECK(get_int(s, IPPROTO_IP, IP_MULTICAST_LOOP) == 0, "mc loop get");
	CHECK(setsockopt_ipv4_multicast_loop(s, 1) == 0, "mc loop on");
	CHECK(get_int(s, IPPROTO_IP, IP_MULTICAST_LOOP) == 1, "mc loop get");

	CHECK(sockopt_reuseaddr(s) == 0, "reuseaddr");
	CHECK(get_int(s, SOL_SOCKET, SO_REUSEADDR) != 0, "reuseaddr readback");
	CHECK(sockopt_reuseaddr(-1) == -1, "reuseaddr bad fd");

#ifdef SO_REUSEPORT
	CHECK(sockopt_reuseport(s) == 0, "reuseport");
	CHECK(get_int(s, SOL_SOCKET, SO_REUSEPORT) != 0, "reuseport readback");
	CHECK(sockopt_reuseport(-1) == -1, "reuseport bad fd");
#endif

	errno = 0;
	CHECK(sockopt_minttl(AF_UNSPEC, s, 3) == -1 && errno == EOPNOTSUPP,
	      "minttl unsupported family");

	close(s);
}

static void test_ipv6_options(void)
{
	int s = socket(AF_INET6, SOCK_DGRAM, 0);

	if (s < 0) {
		printf("no IPv6, skipping\n");
		return;
	}

	CHECK(sockopt_ttl(AF_INET6, s, 33) == 0, "v6 ttl");
	CHECK(get_int(s, IPPROTO_IPV6, IPV6_UNICAST_HOPS) == 33, "v6 ttl get");
	CHECK(setsockopt_ipv6_unicast_hops(s, 44) == 0, "unicast hops");
	CHECK(get_int(s, IPPROTO_IPV6, IPV6_UNICAST_HOPS) == 44, "uh get");
	CHECK(setsockopt_ipv6_multicast_hops(s, 12) == 0, "mc hops");
	CHECK(get_int(s, IPPROTO_IPV6, IPV6_MULTICAST_HOPS) == 12, "mh get");
	CHECK(setsockopt_ipv6_multicast_loop(s, 0) == 0, "v6 mc loop");
	CHECK(get_int(s, IPPROTO_IPV6, IPV6_MULTICAST_LOOP) == 0, "ml get");
	CHECK(setsockopt_ipv6_tclass(s, 0x20) == 0, "tclass");
	CHECK(get_int(s, IPPROTO_IPV6, IPV6_TCLASS) == 0x20, "tclass get");
	CHECK(setsockopt_ipv6_pktinfo(s, 1) == 0, "pktinfo");
	CHECK(setsockopt_ipv6_hoplimit(s, 1) == 0, "hoplimit");
	CHECK(sockopt_v6only(AF_INET6, s) == 0, "v6only");
	CHECK(get_int(s, IPPROTO_IPV6, IPV6_V6ONLY) == 1, "v6only get");
	CHECK(sockopt_ttl(AF_INET6, -1, 33) == -1, "v6 ttl bad fd");
	/* v6only is a no-op for other families */
	CHECK(sockopt_v6only(AF_INET, s) == 0, "v6only other family");

	close(s);
}

static void test_tcp_options(void)
{
	int s = socket(AF_INET, SOCK_STREAM, 0);

	CHECK(sockopt_tcp_mss_set(s, 1000) == 0, "mss set");
	CHECK(sockopt_tcp_mss_get(s) == 1000, "mss get %d",
	      sockopt_tcp_mss_get(s));
	CHECK(sockopt_tcp_mss_get(-1) == 0, "mss get negative fd");
	CHECK(sockopt_tcp_mss_get(open("/dev/null", O_RDONLY)) == 0,
	      "mss get non-socket");

	CHECK(setsockopt_tcp_keepalive(s, 30, 5, 3) == 0, "keepalive");
	CHECK(get_int(s, SOL_SOCKET, SO_KEEPALIVE) != 0, "keepalive get");
#ifdef TCP_KEEPIDLE
	CHECK(get_int(s, IPPROTO_TCP, TCP_KEEPIDLE) == 30, "keepidle");
	CHECK(get_int(s, IPPROTO_TCP, TCP_KEEPINTVL) == 5, "keepintvl");
	CHECK(get_int(s, IPPROTO_TCP, TCP_KEEPCNT) == 3, "keepcnt");
#endif
	CHECK(setsockopt_tcp_keepalive(-1, 30, 5, 3) == -1, "keepalive bad");

	CHECK(sockopt_tcp_rtt(s) >= 0, "rtt on fresh socket");
	close(s);
}

static void test_hdrincl_swab(void)
{
	struct ip iph;

	memset(&iph, 0, sizeof(iph));
	iph.ip_len = 0x0102;
	iph.ip_off = 0x0304;
	iph.ip_id = 0x0506;

	sockopt_iphdrincl_swab_htosys(&iph);
	sockopt_iphdrincl_swab_systoh(&iph);
	CHECK(iph.ip_len == 0x0102 && iph.ip_off == 0x0304
		      && iph.ip_id == 0x0506,
	      "swab roundtrip: %x %x %x", iph.ip_len, iph.ip_off, iph.ip_id);

	sockopt_iphdrincl_swab_htosys(&iph);
	CHECK(iph.ip_id == htons(0x0506), "ip_id converted");
}

int main(void)
{
	test_buffers();
	test_ip_options();
	test_ipv6_options();
	test_tcp_options();
	test_hdrincl_swab();

	printf("%s\n", fail ? "FAILED" : "OK");
	return fail;
}
