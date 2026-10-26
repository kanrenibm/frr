// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * md5 / sha256 / hmac / pbkdf2 known-answer tests
 */
#include <zebra.h>
#include "md5.h"
#include "sha256.h"

static int fail;

static void hex(const uint8_t *d, size_t n, char *out)
{
	for (size_t i = 0; i < n; i++)
		sprintf(out + 2 * i, "%02x", d[i]);
}

static void check(const char *name, const uint8_t *d, size_t n,
		  const char *expect)
{
	char buf[129];

	hex(d, n, buf);
	if (strcmp(buf, expect)) {
		printf("%s: FAILED got %s expected %s\n", name, buf, expect);
		fail = 1;
	} else
		printf("%s: ok\n", name);
}

static void md5_of(const char *name, const char *in, const char *expect)
{
	md5_ctxt ctx;
	uint8_t d[16];

	MD5Init(&ctx);
	MD5Update(&ctx, in, strlen(in));
	MD5Final(d, &ctx);
	check(name, d, sizeof(d), expect);
}

static void sha_of(const char *name, const char *in, const char *expect)
{
	SHA256_CTX ctx;
	uint8_t d[32];

	SHA256_Init(&ctx);
	SHA256_Update(&ctx, in, strlen(in));
	SHA256_Final(d, &ctx);
	check(name, d, sizeof(d), expect);
}

int main(int argc, char **argv)
{
	/* RFC 1321 test suite */
	md5_of("md5-empty", "", "d41d8cd98f00b204e9800998ecf8427e");
	md5_of("md5-a", "a", "0cc175b9c0f1b6a831c399e269772661");
	md5_of("md5-abc", "abc", "900150983cd24fb0d6963f7d28e17f72");
	md5_of("md5-msg", "message digest", "f96b697d7cb7938d525a2f31aaf161d0");
	md5_of("md5-alpha", "abcdefghijklmnopqrstuvwxyz",
	       "c3fcd3d76192e4007dfb496cca67e13b");
	md5_of("md5-80digits",
	       "1234567890123456789012345678901234567890"
	       "1234567890123456789012345678901234567890",
	       "57edf4a22be3c955ac49da2e2107b67a");

	/* incremental update equals one-shot */
	{
		md5_ctxt ctx;
		uint8_t d[16];

		MD5Init(&ctx);
		MD5Update(&ctx, "mess", 4);
		MD5Update(&ctx, "age ", 4);
		MD5Update(&ctx, "digest", 6);
		MD5Final(d, &ctx);
		check("md5-incremental", d, sizeof(d),
		      "f96b697d7cb7938d525a2f31aaf161d0");
	}

	/* RFC 2202 HMAC-MD5 test case 1 and 2 */
	{
		uint8_t d[16];
		unsigned char key1[16];

		memset(key1, 0x0b, sizeof(key1));
		hmac_md5((unsigned char *)"Hi There", 8, key1, 16, d);
		check("hmac-md5-1", d, sizeof(d),
		      "9294727a3638bb1c13f48ef8158bfc9d");
		hmac_md5((unsigned char *)"what do ya want for nothing?", 28,
			 (unsigned char *)"Jefe", 4, d);
		check("hmac-md5-2", d, sizeof(d),
		      "750c783e6ab0b503eaa86e310a5db738");
	}

	/* FIPS 180-2 */
	sha_of("sha256-empty", "",
	       "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
	sha_of("sha256-abc", "abc",
	       "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
	sha_of("sha256-448bits",
	       "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
	       "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

	/* incremental across the 64-byte block boundary */
	{
		SHA256_CTX ctx;
		uint8_t d[32];
		char blk[100];

		memset(blk, 'a', sizeof(blk));
		SHA256_Init(&ctx);
		SHA256_Update(&ctx, blk, 63);
		SHA256_Update(&ctx, blk, 37);
		SHA256_Final(d, &ctx);

		SHA256_CTX ctx2;
		uint8_t d2[32];

		SHA256_Init(&ctx2);
		SHA256_Update(&ctx2, blk, 100);
		SHA256_Final(d2, &ctx2);
		if (memcmp(d, d2, 32)) {
			printf("sha256-incremental: FAILED\n");
			fail = 1;
		} else
			printf("sha256-incremental: ok\n");
	}

	/* RFC 4231 HMAC-SHA256 test case 1 and 2 */
	{
		HMAC_SHA256_CTX ctx;
		uint8_t d[32];
		unsigned char key1[20];

		memset(key1, 0x0b, sizeof(key1));
		HMAC__SHA256_Init(&ctx, key1, sizeof(key1));
		HMAC__SHA256_Update(&ctx, "Hi There", 8);
		HMAC__SHA256_Final(d, &ctx);
		check("hmac-sha256-1", d, sizeof(d),
		      "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");

		HMAC__SHA256_Init(&ctx, "Jefe", 4);
		HMAC__SHA256_Update(&ctx, "what do ya want ", 16);
		HMAC__SHA256_Update(&ctx, "for nothing?", 12);
		HMAC__SHA256_Final(d, &ctx);
		check("hmac-sha256-2", d, sizeof(d),
		      "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
	}

	/* PBKDF2-HMAC-SHA256 vectors (RFC 7914 section 11) */
	{
		uint8_t d[64];

		PBKDF2_SHA256((const uint8_t *)"passwd", 6,
			      (const uint8_t *)"salt", 4, 1, d, 64);
		check("pbkdf2-1", d, 64,
		      "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc"
		      "49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783");
	}

	printf("final tally: %s\n", fail ? "FAILED" : "ok");
	return fail;
}
