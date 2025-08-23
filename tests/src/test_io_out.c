// SPDX-FileCopyrightText: 2019–2025 Andy Curtis <contactandyc@gmail.com>
// SPDX-FileCopyrightText: 2024–2025 Knode.ai — technical questions: contact Andy (above)
// SPDX-License-Identifier: Apache-2.0

// test_io_out.c
#include "the-macro-library/macro_test.h"

#include "the-io-library/io_out.h"
#include "the-io-library/io_in.h"
#include "the-io-library/io.h"
#include "a-memory-library/aml_alloc.h"

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>

static char *mktempdir(void) {
    char buf[] = "/tmp/ioout_test_XXXXXX";
    char *d = mkdtemp(buf);
    MACRO_ASSERT_TRUE(d != NULL);
    return aml_strdup(d);
}

static void path_join(char *dst, const char *a, const char *b) {
    snprintf(dst, PATH_MAX, "%s/%s", a, b);
}

MACRO_TEST(io_out_options_and_basic_write_record_delimited) {
    char *td = mktempdir();
    char f[PATH_MAX]; path_join(f, td, "out.txt");

    io_out_options_t opt;
    io_out_options_init(&opt);
    io_out_options_buffer_size(&opt, 32);
    io_out_options_format(&opt, io_delimiter('\n'));
    io_out_options_safe_mode(&opt);        /* exercise API */
    io_out_options_abort_on_error(&opt);   /* exercise API */

    io_out_t *out = io_out_init(f, &opt);
    MACRO_ASSERT_TRUE(out != NULL);

    MACRO_ASSERT_TRUE(io_out_write_record(out, "a", 1));
    MACRO_ASSERT_TRUE(io_out_write_record(out, "bb", 2));
    MACRO_ASSERT_TRUE(io_out_write_record(out, "ccc", 3));

    io_out_destroy(out);

    /* read back via io_in to validate */
    io_in_t *in = io_in_quick_init(f, io_delimiter('\n'), 16);
    MACRO_ASSERT_TRUE(in != NULL);
    io_record_t *r = io_in_advance(in); MACRO_ASSERT_TRUE(r && r->length==1 && r->record[0]=='a');
    r = io_in_advance(in);              MACRO_ASSERT_TRUE(r && r->length==2);
    r = io_in_advance(in);              MACRO_ASSERT_TRUE(r && r->length==3);
    MACRO_ASSERT_TRUE(io_in_advance(in) == NULL);
    io_in_destroy(in);

    unlink(f); rmdir(td); aml_free(td);
}

MACRO_TEST(io_out_write_and_write_delimiter_with_fd_owner) {
    char *td = mktempdir();
    char f[PATH_MAX]; path_join(f, td, "raw.txt");

    int fd = open(f, O_CREAT|O_TRUNC|O_WRONLY, 0644);
    MACRO_ASSERT_TRUE(fd >= 0);

    io_out_options_t opt;
    io_out_options_init(&opt);
    io_out_options_format(&opt, io_delimiter('\n'));

    io_out_t *out = io_out_init_with_fd(fd, /*fd_owner=*/true, &opt);
    MACRO_ASSERT_TRUE(out != NULL);

    MACRO_ASSERT_TRUE(io_out_write(out, "abc", 3));
    MACRO_ASSERT_TRUE(io_out_write_delimiter(out, "", 0, '\n'));
    MACRO_ASSERT_TRUE(io_out_write_prefix(out, "def", 3)); /* still allowed on single-file mode? exercise call */

    io_out_destroy(out);

    /* validate content starts with "abc\n" */
    size_t len = 0;
    char *content = io_read_file(&len, f);
    MACRO_ASSERT_TRUE(len >= 4);
    MACRO_ASSERT_TRUE(memcmp(content, "abc\n", 4) == 0);
    aml_free(content);

    unlink(f); rmdir(td); aml_free(td);
}

MACRO_TEST(io_out_options_lz4_gz_and_ext_options_api_surface) {
    /* Exercise options API without actually writing compressed/partitioned output */
    io_out_options_t o;
    io_out_options_init(&o);
    io_out_options_lz4(&o, /*level=*/1, /*block size*/ s64kb, /*block_checksum*/false, /*content_checksum*/false);
    io_out_options_gz(&o, 1);
    io_out_options_write_ack_file(&o);
    io_out_options_append_mode(&o);

    io_out_ext_options_t x;
    io_out_ext_options_init(&x);
    io_out_ext_options_use_extra_thread(&x);
    io_out_ext_options_dont_compress_tmp(&x);
    io_out_ext_options_sort_before_partitioning(&x);
    io_out_ext_options_sort_while_partitioning(&x);
    io_out_ext_options_num_sort_threads(&x, 2);
    io_out_ext_options_intermediate_group_size(&x, 4);

    /* simple partition/compare/reducer placeholders */
    io_out_ext_options_partition(&x, NULL, NULL);
    io_out_ext_options_num_partitions(&x, 8);
    io_out_ext_options_compare(&x, NULL, NULL);
    io_out_ext_options_intermediate_compare(&x, NULL, NULL);
    io_out_ext_options_reducer(&x, NULL, NULL);
    io_out_ext_options_intermediate_reducer(&x, NULL, NULL);

    /* fixed-record hooks */
    io_out_ext_options_use_extra_thread(&x);
}

/* --- runner --- */
int main(void) {
    macro_test_case tests[64];
    size_t test_count = 0;

    MACRO_ADD(tests, io_out_options_and_basic_write_record_delimited);
    MACRO_ADD(tests, io_out_write_and_write_delimiter_with_fd_owner);
    MACRO_ADD(tests, io_out_options_lz4_gz_and_ext_options_api_surface);

    macro_run_all("the-io-library/io_out.h", tests, test_count);
    return 0;
}
