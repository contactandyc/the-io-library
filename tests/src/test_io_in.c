// SPDX-FileCopyrightText: 2019–2025 Andy Curtis <contactandyc@gmail.com>
// SPDX-FileCopyrightText: 2024–2025 Knode.ai — technical questions: contact Andy (above)
// SPDX-License-Identifier: Apache-2.0

// test_io_in.c
#include "the-macro-library/macro_test.h"

#include "the-io-library/io_in.h"
#include "the-io-library/io.h"
#include "a-memory-library/aml_alloc.h"
#include "a-memory-library/aml_pool.h"

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>

static char *mktempdir(void) {
    char buf[] = "/tmp/ioin_test_XXXXXX";
    char *d = mkdtemp(buf);
    MACRO_ASSERT_TRUE(d != NULL);
    return aml_strdup(d);
}

static void write_file(const char *path, const void *data, size_t len) {
    int fd = open(path, O_CREAT|O_TRUNC|O_WRONLY, 0644);
    MACRO_ASSERT_TRUE(fd >= 0);
    ssize_t w = write(fd, data, len);
    MACRO_ASSERT_TRUE((size_t)w == len);
    close(fd);
}

static int cmp_records(const io_record_t *a, const io_record_t *b, void *arg) {
    (void)arg;
    size_t la = a->length, lb = b->length;
    size_t m = la < lb ? la : lb;
    int c = memcmp(a->record, b->record, m);
    if (c) return c;
    if (la != lb) return la < lb ? -1 : 1;
    return 0;
}

MACRO_TEST(io_in_options_and_quick_init_delimited) {
    char *td = mktempdir();
    char f[PATH_MAX]; snprintf(f, sizeof(f), "%s/%s", td, "lines.txt");
    write_file(f, "a\nbb\nccc\n", 10);

    /* options init + buffer size + format */
    io_in_options_t opt;
    io_in_options_init(&opt);
    io_in_options_buffer_size(&opt, 32);
    io_in_options_format(&opt, io_delimiter('\n'));
    io_in_options_allow_partial_records(&opt);
    io_in_options_abort_on_file_not_found(&opt); /* does nothing here, just exercising API */

    io_in_t *in = io_in_init(f, &opt);
    MACRO_ASSERT_TRUE(in != NULL);

    /* basic advancement */
    io_record_t *r = io_in_advance(in);
    MACRO_ASSERT_TRUE(r != NULL);
    MACRO_ASSERT_EQ_INT((int)r->length, 1);
    MACRO_ASSERT_TRUE(memcmp(r->record, "a", 1) == 0);

    io_in_reset(in); /* next advance should return same record again */
    io_record_t *r2 = io_in_advance(in);
    MACRO_ASSERT_TRUE(r2 != NULL);
    MACRO_ASSERT_EQ_INT((int)r2->length, 1);
    MACRO_ASSERT_TRUE(memcmp(r2->record, "a", 1) == 0);

    io_record_t *r3 = io_in_advance(in);
    MACRO_ASSERT_TRUE(r3 != NULL && r3->length == 2);
    io_record_t *r4 = io_in_advance(in);
    MACRO_ASSERT_TRUE(r4 != NULL && r4->length == 3);
    MACRO_ASSERT_TRUE(io_in_advance(in) == NULL); /* EOF */

    io_in_destroy(in);

    /* quick init + limit */
    io_in_t *in2 = io_in_quick_init(f, io_delimiter('\n'), 16);
    MACRO_ASSERT_TRUE(in2 != NULL);
    io_in_limit(in2, 2);
    size_t cnt = io_in_count(in2);
    MACRO_ASSERT_EQ_SZ(cnt, 2);
    io_in_destroy(in2);

    unlink(f); rmdir(td); aml_free(td);
}

MACRO_TEST(io_in_with_buffer_and_records_init) {
    /* buffer-based input */
    const char src[] = "X\nY\n";
    io_in_options_t opt;
    io_in_options_init(&opt);
    io_in_options_format(&opt, io_delimiter('\n'));

    io_in_t *in = io_in_init_with_buffer((void*)src, sizeof(src)-1, false, &opt);
    MACRO_ASSERT_TRUE(in != NULL);
    io_record_t *r = io_in_advance(in);
    MACRO_ASSERT_TRUE(r && r->length == 1 && r->record[0] == 'X');
    r = io_in_advance(in);
    MACRO_ASSERT_TRUE(r && r->length == 1 && r->record[0] == 'Y');
    MACRO_ASSERT_TRUE(io_in_advance(in) == NULL);
    io_in_destroy(in);

    /* records-init */
    io_record_t recs[3];
    recs[0].record = "a";  recs[0].length = 1; recs[0].tag = 0;
    recs[1].record = "bb"; recs[1].length = 2; recs[1].tag = 1;
    recs[2].record = "c";  recs[2].length = 1; recs[2].tag = 2;
    io_in_t *in2 = io_in_records_init(recs, 3, &opt);
    MACRO_ASSERT_TRUE(in2 != NULL);
    size_t n = io_in_count(in2);
    MACRO_ASSERT_EQ_SZ(n, 3);
    io_in_destroy(in2);
}

MACRO_TEST(io_in_ext_merge_and_unique) {
    /* two sorted streams with a duplicate key "a" */
    const char left[]  = "a\nb\n";
    const char right[] = "a\nc\n";

    io_in_options_t o; io_in_options_init(&o);
    io_in_options_format(&o, io_delimiter('\n'));

    io_in_t *l = io_in_init_with_buffer((void*)left,  sizeof(left)-1,  false, &o);
    io_in_t *r = io_in_init_with_buffer((void*)right, sizeof(right)-1, false, &o);
    MACRO_ASSERT_TRUE(l && r);

    io_in_t *ext = io_in_ext_init(cmp_records, NULL, &o);
    MACRO_ASSERT_TRUE(ext != NULL);

    io_in_ext_add(ext, l, 0);
    io_in_ext_add(ext, r, 1);

    size_t num_r = 0;
    io_record_t *rec = io_in_advance_unique(ext, &num_r);
    MACRO_ASSERT_TRUE(rec != NULL);
    MACRO_ASSERT_EQ_SZ(num_r, 2); /* "a" appears in both */
    MACRO_ASSERT_TRUE(rec->length == 1 && rec->record[0] == 'a');

    rec = io_in_advance_unique(ext, &num_r);
    MACRO_ASSERT_TRUE(rec != NULL);
    MACRO_ASSERT_EQ_SZ(num_r, 1);
    MACRO_ASSERT_TRUE(rec->record[0] == 'b' || rec->record[0] == 'c');

    rec = io_in_advance_unique(ext, &num_r);
    MACRO_ASSERT_TRUE(rec != NULL);
    MACRO_ASSERT_EQ_SZ(num_r, 1);
    MACRO_ASSERT_TRUE(rec->record[0] == 'b' || rec->record[0] == 'c');

    MACRO_ASSERT_TRUE(io_in_advance_unique(ext, &num_r) == NULL);

    io_in_destroy(ext); /* should also close individual streams */
}

/* --- runner --- */
int main(void) {
    macro_test_case tests[64];
    size_t test_count = 0;

    MACRO_ADD(tests, io_in_options_and_quick_init_delimited);
    MACRO_ADD(tests, io_in_with_buffer_and_records_init);
    MACRO_ADD(tests, io_in_ext_merge_and_unique);

    macro_run_all("the-io-library/io_in.h", tests, test_count);
    return 0;
}
