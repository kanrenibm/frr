// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * jhash unit tests
 */
#include <zebra.h>

#include "jhash.h"

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

static void check_jhash(const char *key, uint32_t initval, uint32_t expect)
{
	uint32_t got = jhash(key, strlen(key), initval);

	CHECK(got == expect, "jhash(\"%s\", %#x) = %#x, expected %#x", key,
	      initval, got, expect);
}

int main(int argc, char **argv)
{
	/* known vectors, cross-checked against an independent implementation
	 * of the lookup2 algorithm.
	 */
	check_jhash("", 0, 0xbd49d10d);
	check_jhash("", 0x12345678, 0x3df641a9);
	check_jhash("a", 0, 0x29eec818);
	check_jhash("a", 0x12345678, 0xdf462a3e);
	check_jhash("abcd", 0, 0x5ae61fa5);
	check_jhash("abcd", 0x12345678, 0x62208b4d);
	/* exactly one 12-byte block, with empty tail */
	check_jhash("hello world!", 0, 0x1eccb293);
	check_jhash("hello world!", 0x12345678, 0x30fc5bd0);
	/* multiple blocks plus a tail */
	check_jhash("The quick brown fox jumps over the lazy dog", 0,
		    0xfc1558de);
	check_jhash("The quick brown fox jumps over the lazy dog", 0x12345678,
		    0x853faba2);

	/* the 1/2/3 word helpers */
	CHECK(jhash_3words(1, 2, 3, 4) == 0xd1222a50, "jhash_3words vector");
	CHECK(jhash_2words(1, 2, 4) == 0x3d228a43, "jhash_2words vector");
	CHECK(jhash_1word(1, 4) == 0xfa8bd2a8, "jhash_1word vector");
	CHECK(jhash_2words(7, 9, 5) == jhash_3words(7, 9, 0, 5),
	      "2words == 3words with zero");
	CHECK(jhash_1word(7, 5) == jhash_3words(7, 0, 0, 5),
	      "1word == 3words with zeros");

	/* alignment independence: same bytes at any offset hash the same */
	{
		const char *msg = "alignment independent hashing check";
		size_t len = strlen(msg);
		uint8_t buf[64];
		uint32_t ref = jhash(msg, len, 42);

		for (size_t off = 0; off < 8; off++) {
			memcpy(buf + off, msg, len);
			CHECK(jhash(buf + off, len, 42) == ref,
			      "offset %zu changes hash", off);
		}
	}

	/* every tail length 0..23 is sensitive to its last byte and initval */
	{
		uint8_t buf[24];

		for (size_t i = 0; i < sizeof(buf); i++)
			buf[i] = i + 1;
		for (uint32_t len = 1; len <= sizeof(buf); len++) {
			uint32_t h = jhash(buf, len, 0);

			CHECK(h == jhash(buf, len, 0), "len %u not stable",
			      len);
			CHECK(h != jhash(buf, len, 1),
			      "len %u ignores initval", len);
			buf[len - 1] ^= 0x80;
			CHECK(h != jhash(buf, len, 0),
			      "len %u ignores last byte", len);
			buf[len - 1] ^= 0x80;
		}
	}

	/* length is mixed in: zero-prefixed keys of different length differ */
	{
		uint8_t zeros[16] = {0};

		CHECK(jhash(zeros, 4, 0) != jhash(zeros, 8, 0),
		      "length not mixed in");
	}

	/* jhash2 */
	{
		uint32_t w[7] = {1, 2, 3, 4, 5, 6, 7};

		for (uint32_t n = 1; n <= 7; n++) {
			uint32_t h = jhash2(w, n, 9);

			CHECK(h == jhash2(w, n, 9), "jhash2 n=%u not stable",
			      n);
			CHECK(h != jhash2(w, n, 10),
			      "jhash2 n=%u ignores initval", n);
			w[n - 1] ^= 1;
			CHECK(h != jhash2(w, n, 9),
			      "jhash2 n=%u ignores last word", n);
			w[n - 1] ^= 1;
		}
	}

	printf("%s\n", fail ? "FAILED" : "all jhash tests passed");
	return fail;
}
