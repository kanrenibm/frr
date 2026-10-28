// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * keychain tests (lib/keychain.c): keychain/key lifecycle, sorted key
 * order, lifetime-aware lookups and the hash algorithm helpers.
 */
#include <zebra.h>

#include "keychain.h"
#include "linklist.h"
#include "qobj.h"

static int fail;

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL: line %d: %s\n", __LINE__, #cond);        \
			fail = 1;                                              \
		}                                                              \
	} while (0)

static void test_keychain_lifecycle(void)
{
	struct keychain *a, *b;

	CHECK(keychain_lookup(NULL) == NULL);
	CHECK(keychain_lookup("a") == NULL);

	a = keychain_get("a");
	CHECK(a != NULL);
	CHECK(strcmp(a->name, "a") == 0);
	CHECK(keychain_lookup("a") == a);
	/* keychain_get() is idempotent */
	CHECK(keychain_get("a") == a);
	CHECK(listcount(keychain_list) == 1);

	b = keychain_get("b");
	CHECK(b != a);
	CHECK(listcount(keychain_list) == 2);

	keychain_delete(a);
	CHECK(keychain_lookup("a") == NULL);
	CHECK(keychain_lookup("b") == b);
	CHECK(listcount(keychain_list) == 1);

	keychain_delete(b);
	CHECK(listcount(keychain_list) == 0);
}

static void test_key_ordering(void)
{
	struct keychain *kc = keychain_get("order");
	struct key *k5, *k1, *k3;
	struct listnode *node;
	struct key *key;
	uint32_t expect[] = {1, 3, 5};
	size_t i = 0;

	k5 = key_get(kc, 5);
	k1 = key_get(kc, 1);
	k3 = key_get(kc, 3);
	CHECK(k5->index == 5 && k1->index == 1 && k3->index == 3);
	CHECK(k1->hash_algo == KEYCHAIN_ALGO_NULL);
	CHECK(k1->string == NULL);
	CHECK(key_get(kc, 3) == k3);
	CHECK(listcount(kc->key) == 3);

	/* keys are kept sorted by index */
	for (ALL_LIST_ELEMENTS_RO(kc->key, node, key)) {
		CHECK(i < array_size(expect));
		if (i < array_size(expect))
			CHECK(key->index == expect[i]);
		i++;
	}

	CHECK(key_lookup(kc, 3) == k3);
	CHECK(key_lookup(kc, 4) == NULL);

	k3->string = XSTRDUP(MTYPE_KEY, "secret");
	key_delete(kc, k3);
	CHECK(key_lookup(kc, 3) == NULL);
	CHECK(listcount(kc->key) == 2);

	/* keychain_delete() frees the remaining keys through the list's del */
	keychain_delete(kc);
}

static void test_lookup_for_accept(void)
{
	struct keychain *kc = keychain_get("accept");
	struct key *k1 = key_get(kc, 1);
	struct key *k2 = key_get(kc, 2);
	struct key *k3 = key_get(kc, 3);
	time_t now = time(NULL);

	/* k1: expired, k2: not yet valid, k3: always valid (start == 0) */
	k1->accept.start = now - 1000;
	k1->accept.end = now - 500;
	k2->accept.start = now + 1000;
	k2->accept.end = now + 2000;

	/* exact match honours the lifetime */
	CHECK(key_lookup_for_accept(kc, 1, true) == NULL);
	CHECK(key_lookup_for_accept(kc, 2, true) == NULL);
	CHECK(key_lookup_for_accept(kc, 3, true) == k3);
	CHECK(key_lookup_for_accept(kc, 4, true) == NULL);

	/* non-exact returns the first valid key at or above the index */
	CHECK(key_lookup_for_accept(kc, 1, false) == k3);
	CHECK(key_lookup_for_accept(kc, 3, false) == k3);
	CHECK(key_lookup_for_accept(kc, 4, false) == NULL);

	/* infinite end (-1) is valid once started */
	k1->accept.start = now - 1000;
	k1->accept.end = (time_t)-1;
	CHECK(key_lookup_for_accept(kc, 1, true) == k1);
	CHECK(key_lookup_for_accept(kc, 1, false) == k1);

	keychain_delete(kc);
}

static void test_match_for_accept(void)
{
	struct keychain *kc = keychain_get("match");
	struct key *k1 = key_get(kc, 1);
	struct key *k2 = key_get(kc, 2);
	struct key *k3 = key_get(kc, 3);
	time_t now = time(NULL);

	/* a key without a string never matches */
	CHECK(key_match_for_accept(kc, "alpha") == NULL);

	k1->string = XSTRDUP(MTYPE_KEY, "alpha");
	k2->string = XSTRDUP(MTYPE_KEY, "beta");
	k3->string = XSTRDUP(MTYPE_KEY, "gamma");

	CHECK(key_match_for_accept(kc, "alpha") == k1);
	CHECK(key_match_for_accept(kc, "beta") == k2);
	CHECK(key_match_for_accept(kc, "delta") == NULL);

	/* expired keys are skipped */
	k2->accept.start = now - 1000;
	k2->accept.end = now - 500;
	CHECK(key_match_for_accept(kc, "beta") == NULL);

	/* only the first 16 bytes are compared */
	k3->string = XREALLOC(MTYPE_KEY, k3->string, 24);
	strlcpy(k3->string, "0123456789abcdefXYZ", 24);
	CHECK(key_match_for_accept(kc, "0123456789abcdefQQQ") == k3);
	CHECK(key_match_for_accept(kc, "0123456789abcdeX") == NULL);

	keychain_delete(kc);
}

static void test_lookup_for_send(void)
{
	struct keychain *kc = keychain_get("send");
	struct key *k1, *k2, *k3;
	struct timespec ts;
	time_t now = time(NULL);

	CHECK(key_lookup_for_send(kc) == NULL);
	ts.tv_sec = now;
	ts.tv_nsec = 0;
	CHECK(keychain_key_find(kc, &ts) == NULL);
	CHECK(keychain_key_find(NULL, &ts) == NULL);
	CHECK(keychain_key_find(kc, NULL) == NULL);

	k1 = key_get(kc, 1);
	k2 = key_get(kc, 2);
	k3 = key_get(kc, 3);

	k1->send.start = now - 1000;
	k1->send.end = now - 500;
	k2->send.start = now + 1000;
	k2->send.end = now + 2000;

	/* k3 has start == 0, so it is the only usable key */
	CHECK(key_lookup_for_send(kc) == k3);
	CHECK(keychain_key_find(kc, &ts) == k3);

	/* at a time inside k1's lifetime keychain_key_find() picks k1 */
	ts.tv_sec = now - 700;
	CHECK(keychain_key_find(kc, &ts) == k1);
	/* inside k2's lifetime (but k3 is always valid, k2 comes first) */
	ts.tv_sec = now + 1500;
	CHECK(keychain_key_find(kc, &ts) == k2);

	/* infinite end */
	k1->send.start = now - 1000;
	k1->send.end = (time_t)-1;
	CHECK(key_lookup_for_send(kc) == k1);

	keychain_delete(kc);
}

static void test_algo_helpers(void)
{
	/* "md5" and "cleartext" resolve identically with either crypto backend */
	CHECK(keychain_get_algo_id_by_name("md5") == KEYCHAIN_ALGO_MD5);
	CHECK(keychain_get_algo_id_by_name("cleartext") == KEYCHAIN_ALGO_CLEARTEXT);
	CHECK(keychain_get_algo_id_by_name("hmac-sha-256") == KEYCHAIN_ALGO_HMAC_SHA256);
	CHECK(keychain_get_algo_id_by_name("bogus") == KEYCHAIN_ALGO_NULL);
	CHECK(keychain_get_algo_id_by_name("") == KEYCHAIN_ALGO_NULL);

	CHECK(strcmp(keychain_get_algo_name_by_id(KEYCHAIN_ALGO_MD5), "md5") == 0);
	CHECK(strcmp(keychain_get_algo_name_by_id(KEYCHAIN_ALGO_HMAC_SHA256), "hmac-sha-256") == 0);
	CHECK(strcmp(keychain_get_algo_name_by_id(KEYCHAIN_ALGO_NULL), "null") == 0);

	CHECK(strcmp(keychain_get_description(KEYCHAIN_ALGO_MD5), "MD5") == 0);
	CHECK(strcmp(keychain_get_description(KEYCHAIN_ALGO_HMAC_SHA512), "HMAC-SHA-512") == 0);

	CHECK(keychain_get_hash_len(KEYCHAIN_ALGO_MD5) == KEYCHAIN_MD5_HASH_SIZE);
	CHECK(keychain_get_hash_len(KEYCHAIN_ALGO_HMAC_SHA1) == KEYCHAIN_HMAC_SHA1_HASH_SIZE);
	CHECK(keychain_get_hash_len(KEYCHAIN_ALGO_HMAC_SHA256) == KEYCHAIN_HMAC_SHA256_HASH_SIZE);
	CHECK(keychain_get_hash_len(KEYCHAIN_ALGO_NULL) == 0);
	CHECK(keychain_get_hash_len(KEYCHAIN_ALGO_CLEARTEXT) == 0);

	CHECK(keychain_get_block_size(KEYCHAIN_ALGO_MD5) == KEYCHAIN_ALGO_MD5_INTERNAL_BLK_SIZE);
	CHECK(keychain_get_block_size(KEYCHAIN_ALGO_HMAC_SHA384) == KEYCHAIN_ALGO_SHA384_INTERNAL_BLK_SIZE);

	CHECK(keychain_get_hash_algo_info(KEYCHAIN_ALGO_HMAC_SHA1).key == KEYCHAIN_ALGO_HMAC_SHA1);
	CHECK(keychain_get_hash_algo_info(KEYCHAIN_ALGO_MAX).key == KEYCHAIN_ALGO_MAX);
}

int main(int argc, char **argv)
{
	qobj_init();
	/* headless init: no CLI/northbound registration */
	keychain_list = list_new();

	test_keychain_lifecycle();
	test_key_ordering();
	test_lookup_for_accept();
	test_match_for_accept();
	test_lookup_for_send();
	test_algo_helpers();

	keychain_terminate();
	qobj_finish();

	if (fail) {
		printf("keychain tests FAILED\n");
		return 1;
	}
	printf("keychain tests passed\n");
	return 0;
}
