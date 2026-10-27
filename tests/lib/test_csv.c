// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * lib/csv encode/decode/serialize/record-manipulation tests
 */
#include <zebra.h>

#include "csv.h"

static int fail;

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL: line %d: %s\n", __LINE__, #cond);        \
			fail = 1;                                              \
		}                                                              \
	} while (0)

/* fields are not individually NUL terminated, compare using field_len */
static bool fld_is(csv_field_t *fld, const char *str)
{
	return fld && csv_field_len(fld) == (int)strlen(str);
}

static bool rec_is(csv_record_t *rec, int count, const char **expect)
{
	csv_field_t *fld = NULL;
	char *s = csv_field_iter(rec, &fld);
	int i;

	for (i = 0; i < count; i++) {
		if (!s || !fld_is(fld, expect[i])
		    || strncmp(s, expect[i], strlen(expect[i])))
			return false;
		s = csv_field_iter_next(&fld);
	}
	return s == NULL;
}

static void test_single_buf_encode(void)
{
	char buf[128] = "";
	char out[128];
	csv_t csv;
	csv_record_t *rec, *r2;
	const char *exp1[] = {"name", "age", "sex"};

	CHECK(csv_init(&csv, buf, sizeof(buf)) == &csv);
	CHECK(csvlen(&csv) == 0);
	CHECK(csv_num_records(&csv) == 0);
	CHECK(csv_num_records(NULL) == 0);
	CHECK(csv_record_iter(&csv) == NULL);

	rec = csv_encode(&csv, 3, "name", "age", "sex");
	CHECK(rec != NULL);
	CHECK(!strcmp(buf, "name,age,sex\n"));
	CHECK(csvlen(&csv) == 13);
	CHECK(csv_num_records(&csv) == 1);
	CHECK(rec_is(rec, 3, exp1));

	r2 = csv_encode(&csv, 2, "p", "35");
	CHECK(r2 != NULL);
	CHECK(!strcmp(buf, "name,age,sex\np,35\n"));
	CHECK(csvlen(&csv) == 18);
	CHECK(csv_num_records(&csv) == 2);
	CHECK(csv_record_iter(&csv) == rec);
	CHECK(csv_record_iter_next(rec) == r2);
	CHECK(csv_record_iter_next(r2) == NULL);
	CHECK(csv_record_iter_next(NULL) == NULL);

	CHECK(csv_is_record_valid(&csv, rec));
	CHECK(csv_is_record_valid(&csv, r2));
	CHECK(csv_is_record_valid(&csv, NULL) == 0);

	/* serialize: success, then buffer too small */
	CHECK(csv_serialize(&csv, out, sizeof(out)) == 0);
	CHECK(!strcmp(out, "name,age,sex\np,35\n"));
	CHECK(csv_serialize(&csv, out, 10) == -1);
	CHECK(csv_serialize(&csv, NULL, sizeof(out)) == -1);
	CHECK(csv_serialize(NULL, out, sizeof(out)) == -1);

	/* operations that need discrete buffers refuse a single buf csv */
	CHECK(csv_append_record(&csv, rec, 1, "x") == NULL);
	CHECK(csv_concat_record(&csv, rec, r2) == NULL);

	/* in-place overwrite keeps field widths */
	CHECK(csv_encode_record(&csv, r2, 2, "q", "99") == r2);
	CHECK(!strcmp(buf, "name,age,sex\nq,99\n"));

	csv_remove_record(&csv, rec);
	CHECK(csv_num_records(&csv) == 1);
	CHECK(csvlen(&csv) == 5);
	CHECK(csv_is_record_valid(&csv, rec) == 0);

	csv_clean(&csv);
	CHECK(csv_num_records(&csv) == 0);
	CHECK(csvlen(&csv) == 0);
	CHECK(csv_record_iter(&csv) == NULL);
}

static void test_buffer_truncation(void)
{
	char buf[8] = "";
	csv_t csv;
	csv_record_t *rec;

	csv_init(&csv, buf, sizeof(buf));
	rec = csv_encode(&csv, 2, "abcdefghij", "k");
	CHECK(rec != NULL);
	/* output never overruns the supplied buffer */
	CHECK(strlen(buf) < sizeof(buf));
	csv_clean(&csv);
}

static void test_decode(void)
{
	char buf[] = "a,b,,c\n";
	csv_t csv;
	csv_record_t *rec;
	csv_field_t *fld = NULL;
	char *s;
	const char *exp[] = {"a", "b", "", "c"};

	/* decode in place from the csv's own buffer */
	csv_init(&csv, buf, sizeof(buf));
	csv_decode(&csv, NULL);
	CHECK(csv_num_records(&csv) == 1);

	rec = csv_record_iter(&csv);
	CHECK(rec_is(rec, 4, exp));

	CHECK(csv_field_len(NULL) == 0);
	CHECK(csv_field_iter(NULL, &fld) == NULL);
	s = csv_field_iter(rec, &fld);
	CHECK(s == buf);

	csv_clean(&csv);
	CHECK(csv_num_records(&csv) == 0);
}

static void test_discrete_decode_and_clone(void)
{
	char in[] = "k1,v1\nk2,v2\n,x,\n";
	csv_t *csv = csv_init(NULL, NULL, 64);
	csv_record_t *rec, *clone = NULL;
	const char *exp[] = {"k1", "v1"};
	const char *exp_empty[] = {"", "x", ""};

	CHECK(csv != NULL);
	csv_decode(csv, in);
	CHECK(csv_num_records(csv) == 3);
	CHECK(rec_is(csv_record_iter_next(csv_record_iter_next(
			     csv_record_iter(csv))),
		     3, exp_empty));

	rec = csv_record_iter(csv);
	CHECK(rec_is(rec, 2, exp));

	csv_clone_record(csv, rec, &clone);
	CHECK(clone != NULL);
	if (clone) {
		CHECK(clone != rec);
		CHECK(rec_is(clone, 2, exp));
		/* clone is not part of csv until inserted */
		CHECK(csv_is_record_valid(csv, clone) == 0);
		csv_insert_record(csv, clone);
		CHECK(csv_is_record_valid(csv, clone));
		CHECK(csv_num_records(csv) == 4);
		/* inserting twice is rejected */
		csv_insert_record(csv, clone);
		CHECK(csv_num_records(csv) == 4);
	}

	csv_clean(csv);
	csv_free(csv);
	csv_free(NULL);
}

static void test_discrete_append_concat(void)
{
	csv_t *csv = csv_init(NULL, NULL, 64);
	csv_record_t *r1, *r2, *joined, *app;
	const char *exp[] = {"a", "b", "c", "d"};
	const char *exp_app[] = {"x", "y", "z"};
	char out[64];

	CHECK(csv != NULL);
	r1 = csv_encode(csv, 2, "a", "b");
	r2 = csv_encode(csv, 2, "c", "d");
	CHECK(r1 && r2);
	CHECK(csv_num_records(csv) == 2);

	joined = csv_concat_record(csv, r1, r2);
	CHECK(joined != NULL);
	CHECK(csv_num_records(csv) == 1);
	CHECK(rec_is(joined, 4, exp));
	CHECK(csv_serialize(csv, out, sizeof(out)) == 0);
	CHECK(!strcmp(out, "a,b,c,d\n"));

	/* a record that is not in the csv is rejected */
	CHECK(csv_concat_record(csv, joined, NULL) == NULL);

	/* append creates a new record, then extends it */
	app = csv_append_record(csv, NULL, 2, "x", "y");
	CHECK(app != NULL);
	CHECK(csv_num_records(csv) == 2);
	CHECK(csv_append_record(csv, app, 1, "z") == app);
	CHECK(rec_is(app, 3, exp_app));
	CHECK(csv_serialize(csv, out, sizeof(out)) == 0);
	CHECK(!strcmp(out, "a,b,c,d\nx,y,z\n"));
	CHECK(csvlen(csv) == (int)strlen(out));

	csv_clean(csv);
	csv_free(csv);
}

int main(int argc, char **argv)
{
	test_single_buf_encode();
	test_buffer_truncation();
	test_decode();
	test_discrete_decode_and_clone();
	test_discrete_append_concat();

	if (!fail)
		printf("OK\n");
	return fail;
}
