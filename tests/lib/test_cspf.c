// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Constrained Shortest Path First (lib/cspf.c) tests.
 *
 * Topology (metric / TE metric):
 *
 *        1 --10/20-- 2
 *         \         /
 *        50/5     10/20
 *           \     /
 *              3
 */

#include <zebra.h>

#include "linklist.h"
#include "link_state.h"
#include "cspf.h"

static struct in_addr ipv4(const char *address)
{
	struct in_addr result;

	assert(inet_pton(AF_INET, address, &result) == 1);
	return result;
}

static struct ls_node_id node_id(uint8_t id)
{
	struct ls_node_id result = {
		.origin = ISIS_L1,
		.id.iso.level = 1,
	};

	result.id.iso.sys_id[ISO_SYS_ID_LEN - 1] = id;
	return result;
}

static void add_edge(struct ls_ted *ted, uint8_t adv, const char *local,
		     const char *remote, uint32_t metric, uint32_t te_metric,
		     float bw)
{
	struct ls_attributes *attr;

	attr = ls_attributes_new(node_id(adv), ipv4(local), in6addr_any, 0);
	assert(attr);

	attr->standard.remote = ipv4(remote);
	SET_FLAG(attr->flags, LS_ATTR_NEIGH_ADDR);
	attr->metric = metric;
	SET_FLAG(attr->flags, LS_ATTR_METRIC);
	attr->standard.te_metric = te_metric;
	SET_FLAG(attr->flags, LS_ATTR_TE_METRIC);
	attr->standard.max_bw = bw;
	attr->standard.max_rsv_bw = bw;
	attr->standard.unrsv_bw[0] = bw;
	SET_FLAG(attr->flags, LS_ATTR_MAX_BW);
	SET_FLAG(attr->flags, LS_ATTR_MAX_RSV_BW);
	SET_FLAG(attr->flags, LS_ATTR_UNRSV_BW);

	assert(ls_edge_add(ted, attr));
}

static void add_link(struct ls_ted *ted, uint8_t a, uint8_t b,
		     const char *a_addr, const char *b_addr, uint32_t metric,
		     uint32_t te_metric, float bw)
{
	add_edge(ted, a, a_addr, b_addr, metric, te_metric, bw);
	add_edge(ted, b, b_addr, a_addr, metric, te_metric, bw);
}

static struct ls_ted *build_ted(void)
{
	struct ls_ted *ted = ls_ted_new(1, "cspf-test", 0);

	assert(ted);
	add_link(ted, 1, 2, "10.0.1.1", "10.0.1.2", 10, 20, 0);
	add_link(ted, 2, 3, "10.0.2.2", "10.0.2.3", 10, 20, 0);
	add_link(ted, 1, 3, "10.0.3.1", "10.0.3.3", 50, 5, 1000);
	return ted;
}

static struct ls_vertex *vertex(struct ls_ted *ted, uint8_t id)
{
	struct ls_vertex *v = ls_find_vertex_by_id(ted, node_id(id));

	assert(v);
	return v;
}

static struct c_path *compute(struct ls_ted *ted, uint8_t src, uint8_t dst,
			      struct constraints *csts)
{
	struct cspf *algo;
	struct c_path *path;

	algo = cspf_init(NULL, vertex(ted, src), vertex(ted, dst), csts);
	assert(algo);
	path = compute_p2p_path(algo, ted);
	assert(path);
	cspf_del(algo);
	return path;
}

static uint64_t edge_dst_key(struct c_path *path, unsigned int idx)
{
	struct listnode *node;
	struct ls_edge *edge;

	for (ALL_LIST_ELEMENTS_RO(path->edges, node, edge))
		if (idx-- == 0) {
			assert(edge->destination);
			return edge->destination->key;
		}

	assert(0);
	return 0;
}

static void test_metric_shortest_path(void)
{
	struct ls_ted *ted = build_ted();
	struct constraints csts = {
		.cost = MAX_COST,
		.ctype = CSPF_METRIC,
		.family = AF_INET,
		.type = RSVP_TE,
	};
	struct c_path *path = compute(ted, 1, 3, &csts);

	/* 1->2->3 (20) beats the direct 1->3 link (50) */
	assert(path->status == SUCCESS);
	assert(path->weight == 20);
	assert(listcount(path->edges) == 2);
	assert(edge_dst_key(path, 0) == vertex(ted, 2)->key);
	assert(edge_dst_key(path, 1) == vertex(ted, 3)->key);

	cpath_del(path);
	ls_ted_del_all(&ted);
}

static void test_te_metric_shortest_path(void)
{
	struct ls_ted *ted = build_ted();
	struct constraints csts = {
		.cost = MAX_COST,
		.ctype = CSPF_TE_METRIC,
		.family = AF_INET,
		.type = RSVP_TE,
	};
	struct c_path *path = compute(ted, 1, 3, &csts);

	/* with TE metric the direct link (5) beats 1->2->3 (40) */
	assert(path->status == SUCCESS);
	assert(path->weight == 5);
	assert(listcount(path->edges) == 1);
	assert(edge_dst_key(path, 0) == vertex(ted, 3)->key);

	cpath_del(path);
	ls_ted_del_all(&ted);
}

static void test_cost_limit_prunes_all_paths(void)
{
	struct ls_ted *ted = build_ted();
	struct constraints csts = {
		.cost = 15,
		.ctype = CSPF_METRIC,
		.family = AF_INET,
		.type = RSVP_TE,
	};
	struct c_path *path = compute(ted, 1, 3, &csts);

	/* 1->2 fits, but 1->2->3 (20) and 1->3 (50) exceed the limit */
	assert(path->status == FAILED);
	assert(listcount(path->edges) == 0);

	cpath_del(path);
	ls_ted_del_all(&ted);
}

static void test_bandwidth_constraint(void)
{
	struct ls_ted *ted = build_ted();
	struct constraints csts = {
		.cost = MAX_COST,
		.ctype = CSPF_METRIC,
		.family = AF_INET,
		.type = RSVP_TE,
		.bw = 500.0f,
		.cos = 0,
	};
	struct c_path *path = compute(ted, 1, 3, &csts);

	/* only the direct link has enough bandwidth */
	assert(path->status == SUCCESS);
	assert(path->weight == 50);
	assert(listcount(path->edges) == 1);
	assert(edge_dst_key(path, 0) == vertex(ted, 3)->key);
	cpath_del(path);

	/* nothing can carry this much */
	csts.bw = 5000.0f;
	path = compute(ted, 1, 3, &csts);
	assert(path->status == FAILED);
	cpath_del(path);

	ls_ted_del_all(&ted);
}

static void test_invalid_requests(void)
{
	struct ls_ted *ted = build_ted();
	struct constraints csts = {
		.cost = MAX_COST,
		.ctype = CSPF_METRIC,
		.family = AF_INET,
		.type = RSVP_TE,
	};
	struct constraints no_type = { .cost = MAX_COST, .family = AF_INET };
	struct cspf *algo;
	struct c_path *path;

	assert(cspf_init(NULL, vertex(ted, 1), vertex(ted, 3), NULL) == NULL);

	/* same source and destination */
	path = compute(ted, 1, 1, &csts);
	assert(path->status == SAME_SRC_DST);
	cpath_del(path);

	/* missing destination */
	algo = cspf_init(NULL, vertex(ted, 1), NULL, &csts);
	path = compute_p2p_path(algo, ted);
	assert(path->status == NO_DESTINATION);
	cpath_del(path);
	cspf_del(algo);

	/* missing source */
	algo = cspf_init(NULL, NULL, vertex(ted, 3), &csts);
	path = compute_p2p_path(algo, ted);
	assert(path->status == NO_SOURCE);
	cpath_del(path);
	cspf_del(algo);

	/* no metric type selected */
	path = compute(ted, 1, 3, &no_type);
	assert(path->status == FAILED);
	cpath_del(path);

	/* NULL algo */
	path = compute_p2p_path(NULL, ted);
	assert(path->status == FAILED);
	cpath_del(path);

	ls_ted_del_all(&ted);
}

int main(void)
{
	test_metric_shortest_path();
	test_te_metric_shortest_path();
	test_cost_limit_prunes_all_paths();
	test_bandwidth_constraint();
	test_invalid_requests();

	printf("CSPF tests passed.\n");
	return 0;
}
