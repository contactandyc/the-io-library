// SPDX-FileCopyrightText: 2019–2025 Andy Curtis <contactandyc@gmail.com>
// SPDX-FileCopyrightText: 2024–2025 Knode.ai — technical questions: contact Andy (above)
// SPDX-License-Identifier: Apache-2.0

// test_io.c
#include "the-macro-library/macro_test.h"

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
#include <stdlib.h>
#include <errno.h>

static char *mktempdir(void) {
    char buf[] = "/tmp/ioh_test_XXXXXX";
    char *d = mkdtemp(buf);
    MACRO_ASSERT_TRUE(d != NULL);
    return aml_strdup(d);
}

static void path_join(char *dst, const char *a, const char *b) {
    snprintf(dst, PATH_MAX, "%s/%s", a, b);
}

static void write_file(const char *path, const void *data, size_t len) {
    int fd = open(path, O_CREAT|O_TRUNC|O_WRONLY, 0644);
    MACRO_ASSERT_TRUE(fd >= 0);
    ssize_t w = write(fd, data, len);
    MACRO_ASSERT_TRUE((size_t)w == len);
    close(fd);
}

static void write_repeated(const char *path, char ch, size_t len) {
    int fd = open(path, O_CREAT|O_TRUNC|O_WRONLY, 0644);
    MACRO_ASSERT_TRUE(fd >= 0);
    size_t chunk = 4096;
    char *buf = (char*)aml_malloc(chunk);
    memset(buf, ch, chunk);
    size_t left = len;
    while (left) {
        size_t tow = left > chunk ? chunk : left;
        ssize_t w = write(fd, buf, tow);
        MACRO_ASSERT_TRUE((size_t)w == tow);
        left -= tow;
    }
    aml_free(buf);
    close(fd);
}

MACRO_TEST(io_formats_and_inline_helpers) {
    /* API presence / basic use */
    (void)io_delimiter('\n');
    (void)io_csv_delimiter(',');
    (void)io_fixed(8);
    (void)io_prefix();

    /* inline compares and split helpers */
    uint32_t a32 = 10, b32 = 20;
    io_record_t r1 = { (char*)&a32, sizeof(a32), 0 };
    io_record_t r2 = { (char*)&b32, sizeof(b32), 0 };
    MACRO_ASSERT_TRUE(io_compare_uint32_t(&r1, &r2, NULL) < 0);
    MACRO_ASSERT_TRUE(io_split_by_uint32_t(&r1, 7, NULL) < 7);

    uint64_t a64v[2] = { 5, 42 }, b64v[2] = { 5, 100 };
    io_record_t r3 = { (char*)a64v, sizeof(uint64_t)*2, 0 };
    io_record_t r4 = { (char*)b64v, sizeof(uint64_t)*2, 0 };
    MACRO_ASSERT_TRUE(io_compare_uint64_t(&r3, &r4, NULL) == 0); /* first 64 equal */
    MACRO_ASSERT_TRUE(io_split_by_uint64_t_2(&r4, 9, NULL) < 9);
}

MACRO_TEST(io_file_and_dir_helpers) {
    char *td = mktempdir();

    char f1[PATH_MAX]; path_join(f1, td, "a.txt");
    write_file(f1, "hello\nworld\n", 12);

    MACRO_ASSERT_TRUE(io_file_exists(f1));
    MACRO_ASSERT_TRUE(io_file(f1));
    MACRO_ASSERT_TRUE(!io_directory(f1));
    MACRO_ASSERT_TRUE(io_file_size(f1) == 12);

    MACRO_ASSERT_TRUE(io_extension(f1, "txt"));
    MACRO_ASSERT_TRUE(!io_extension(f1, "lz4"));
    MACRO_ASSERT_TRUE(io_modified(f1) > 0);

    /* make a nested path valid */
    char nested[PATH_MAX]; path_join(nested, td, "x/y/z/file.dat");
    MACRO_ASSERT_TRUE(io_make_path_valid(nested)); /* creates x/y/z if needed */
    MACRO_ASSERT_TRUE(io_directory(td)); /* sanity */

    /* find file in parents */
    char sub[PATH_MAX]; path_join(sub, td, "x");
    char cwd[PATH_MAX]; getcwd(cwd, sizeof(cwd));
    MACRO_ASSERT_TRUE(chdir(sub) == 0);
    char *found = io_find_file_in_parents("a.txt");
    MACRO_ASSERT_TRUE(found != NULL);
    MACRO_ASSERT_TRUE(strstr(found, "/a.txt") != NULL);
    aml_free(found);
    (void)chdir(cwd);

    unlink(f1);
    rmdir(sub); /* /x */
    /* remove created dirs (y and z) */
    char yz[PATH_MAX]; path_join(yz, td, "x/y/z"); rmdir(yz);
    char y[PATH_MAX];  path_join(y,  td, "x/y");   rmdir(y);
    char x[PATH_MAX];  path_join(x,  td, "x");     rmdir(x);
    rmdir(td);
    aml_free(td);
}

MACRO_TEST(io_read_file_and_chunks) {
    char *td = mktempdir();
    char f[PATH_MAX]; path_join(f, td, "blob.bin");
    const char payload[] = "ABCDEF0123456789";
    write_file(f, payload, sizeof(payload)-1);

    size_t len = 0;
    char *buf = io_read_file(&len, f);
    MACRO_ASSERT_EQ_SZ(len, sizeof(payload)-1);
    MACRO_ASSERT_TRUE(memcmp(buf, payload, len) == 0);
    aml_free(buf);

    /* pool variant */
    aml_pool_t *pool = aml_pool_init(256);
    char *pbuf = io_pool_read_file(pool, &len, f);
    MACRO_ASSERT_EQ_SZ(len, sizeof(payload)-1);
    MACRO_ASSERT_TRUE(memcmp(pbuf, payload, len) == 0);

    /* chunk into buffer */
    char small[8];
    size_t got = 0;
    MACRO_ASSERT_TRUE(io_read_chunk_into_buffer(small, &got, f, 1, 5));
    MACRO_ASSERT_EQ_SZ(got, 5);
    MACRO_ASSERT_TRUE(memcmp(small, "BCDEF", 5) == 0);

    /* aligned read: create file with size multiple of alignment */
    char f2[PATH_MAX]; path_join(f2, td, "aligned.bin");
    size_t align = 4096, total = align * 2; /* 8192 bytes */
    write_repeated(f2, 'Z', total);
    size_t alen = 0;
    char *abuf = io_read_file_aligned(&alen, align, f2);
    MACRO_ASSERT_TRUE(abuf != NULL);
    MACRO_ASSERT_EQ_SZ(alen, total);
    for (size_t i=0;i<alen;i++) MACRO_ASSERT_TRUE(abuf[i] == 'Z');
    free(abuf); /* NOTE: free (not aml_free) per API */

    aml_pool_destroy(pool);
    unlink(f); unlink(f2);
    rmdir(td);
    aml_free(td);
}

static bool all_files_valid(const char *filename, void *arg) { (void)filename; (void)arg; return true; }

static size_t last_char_partition(const io_file_info_t *fi, size_t num_part, void *tag) {
    (void)tag;
    size_t n = strlen(fi->filename);
    if (n == 0) return num_part;
    return (size_t)(fi->filename[n-1] % num_part);
}

MACRO_TEST(io_list_and_sort_file_info) {
    char *td = mktempdir();
    char fa[PATH_MAX]; path_join(fa, td, "a");
    char fb[PATH_MAX]; path_join(fb, td, "b");
    write_file(fa, "1", 1);
    write_file(fb, "2222", 4);

    size_t num=0;
    io_file_info_t *files = io_list(td, &num, all_files_valid, NULL);
    MACRO_ASSERT_TRUE(files != NULL);
    MACRO_ASSERT_TRUE(num >= 2);

    /* sort by filename asc */
    io_sort_file_info_by_filename(files, num);
    for (size_t i=1;i<num;i++)
        MACRO_ASSERT_TRUE(strcmp(files[i-1].filename, files[i].filename) <= 0);

    /* sort by size desc */
    io_sort_file_info_by_size_descending(files, num);
    for (size_t i=1;i<num;i++)
        MACRO_ASSERT_TRUE(files[i-1].size >= files[i].size);

    /* hash filename returns something deterministic (non-zero likely) */
    MACRO_ASSERT_TRUE(io_hash_filename("abc") == io_hash_filename("abc"));

    /* partition selection */
    aml_pool_t *pool = aml_pool_init(256);
    size_t pick=0;
    io_file_info_t *pf = io_partition_file_info(pool, &pick, files, num, 0, 2,
                                                last_char_partition, NULL);
    MACRO_ASSERT_TRUE(pf != NULL); /* may be empty set but pointer returned */

    aml_free(files);
    aml_pool_destroy(pool);

    unlink(fa); unlink(fb);
    rmdir(td);
    aml_free(td);
}

static int cmp_u32_records(const io_record_t *a, const io_record_t *b, void *tag) {
    (void)tag;
    return io_compare_uint32_t(a, b, NULL);
}

MACRO_TEST(io_sort_records_and_hash_partition) {
    uint32_t v3=3, v1=1, v2=2;
    io_record_t arr[3] = {
        { (char*)&v3, sizeof(v3), 0 },
        { (char*)&v1, sizeof(v1), 0 },
        { (char*)&v2, sizeof(v2), 0 }
    };
    io_sort_records(arr, 3, cmp_u32_records, NULL);
    MACRO_ASSERT_TRUE(*(uint32_t*)arr[0].record == 1);
    MACRO_ASSERT_TRUE(*(uint32_t*)arr[1].record == 2);
    MACRO_ASSERT_TRUE(*(uint32_t*)arr[2].record == 3);

    size_t part = io_hash_partition(&arr[0], 7, NULL);
    MACRO_ASSERT_TRUE(part < 7);
}

/* --- runner --- */
int main(void) {
    macro_test_case tests[64];
    size_t test_count = 0;

    MACRO_ADD(tests, io_formats_and_inline_helpers);
    MACRO_ADD(tests, io_file_and_dir_helpers);
    MACRO_ADD(tests, io_read_file_and_chunks);
    MACRO_ADD(tests, io_list_and_sort_file_info);
    MACRO_ADD(tests, io_sort_records_and_hash_partition);

    macro_run_all("the-io-library/io.h", tests, test_count);
    return 0;
}
