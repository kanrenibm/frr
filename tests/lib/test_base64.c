// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * base64 encode/decode tests (RFC 4648 section 10 vectors)
 */
#include <zebra.h>

#include "base64.h"

static int fail;

static void check(bool ok, const char *what, const char *input)
{
	if (!ok) {
		printf("FAIL: %s (input \"%s\")\n", what, input);
		fail = 1;
	}
}

static void encode_one(const char *plain, const char *expect, size_t chunk)
{
	struct base64_encodestate st;
	char out[64] = "";
	size_t len = strlen(plain), pos = 0;
	int n = 0;

	base64_init_encodestate(&st);
	while (pos < len) {
		size_t c = len - pos < chunk ? len - pos : chunk;

		n += base64_encode_block(plain + pos, c, out + n, &st);
		pos += c;
	}
	n += base64_encode_blockend(out + n, &st);
	out[n] = '\0';

	check(!strcmp(out, expect), "encode mismatch", plain);
	printf("encode(\"%s\", chunk=%zu) = \"%s\"\n", plain, chunk, out);
}

static void decode_one(const char *coded, const char *expect, size_t chunk)
{
	struct base64_decodestate st;
	/* decoder writes one byte past the returned length */
	char out[64] = "";
	size_t len = strlen(coded), pos = 0;
	int n = 0;

	base64_init_decodestate(&st);
	while (pos < len) {
		size_t c = len - pos < chunk ? len - pos : chunk;

		n += base64_decode_block(coded + pos, c, out + n, &st);
		pos += c;
	}

	check(n == (int)strlen(expect), "decode length", coded);
	check(!memcmp(out, expect, n), "decode content", coded);
	printf("decode(\"%s\", chunk=%zu) = %d bytes\n", coded, chunk, n);
}

static const struct {
	const char *plain;
	const char *coded;
} vectors[] = {
	{ "", "" },
	{ "f", "Zg==" },
	{ "fo", "Zm8=" },
	{ "foo", "Zm9v" },
	{ "foob", "Zm9vYg==" },
	{ "fooba", "Zm9vYmE=" },
	{ "foobar", "Zm9vYmFy" },
};

int main(int argc, char **argv)
{
	size_t i, chunk;

	/* whole-buffer and byte-at-a-time, to exercise state carry-over */
	for (i = 0; i < array_size(vectors); i++)
		for (chunk = 1; chunk <= 7; chunk += 3) {
			encode_one(vectors[i].plain, vectors[i].coded, chunk);
			decode_one(vectors[i].coded, vectors[i].plain, chunk);
		}

	/* decoder skips characters outside the alphabet, e.g. line breaks */
	decode_one("Zm9v\nYmFy", "foobar", 9);

	check(base64_encode_value(0) == 'A', "encode_value(0)", "");
	check(base64_encode_value(63) == '/', "encode_value(63)", "");
	check(base64_encode_value(64) == '=', "encode_value(64)", "");
	check(base64_decode_value('+') == 62, "decode_value('+')", "");
	check(base64_decode_value('/') == 63, "decode_value('/')", "");
	check(base64_decode_value('A') == 0, "decode_value('A')", "");
	check(base64_decode_value('=') == -2, "decode_value('=')", "");
	check(base64_decode_value('!') == -1, "decode_value('!')", "");
	check(base64_decode_value('~') == -1, "decode_value('~')", "");

	printf("final tally: %s\n", fail ? "FAILED" : "ok");
	return fail;
}
