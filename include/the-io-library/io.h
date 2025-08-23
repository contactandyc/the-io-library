// SPDX-FileCopyrightText: 2019–2025 Andy Curtis <contactandyc@gmail.com>
// SPDX-FileCopyrightText: 2024–2025 Knode.ai — technical questions: contact Andy (above)
// SPDX-License-Identifier: Apache-2.0

#ifndef _io_H
#define _io_H

#include "a-memory-library/aml_alloc.h"
#include "a-memory-library/aml_buffer.h"
#include "the-macro-library/macro_sort.h"

#include <inttypes.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* io_format_t is used to define different file formats for files containing records. */
typedef int io_format_t;

/* the delim should be a character 0..255 and is commonly used for newline ('\n') delimited files */
io_format_t io_delimiter(int delim);
/* used to indicate that a records are in the csv format */
io_format_t io_csv_delimiter(int delim);
/* fixed length records of a given size */
io_format_t io_fixed(int size);
/* variable length records with a 4 byte prefix length */
io_format_t io_prefix(void);

/* common record structure for the io_format's above */
typedef struct {
  char *record;
  uint32_t length;
  int32_t tag;
} io_record_t;


/* a reducer function which expects r, num_r to be consolidated to a single record.  The buffer is used to
   buffer up the num_r records and the tag is user defined */
typedef bool (*io_reducer_cb)(io_record_t *res, const io_record_t *r,
                                size_t num_r, aml_buffer_t *bh, void *tag);

/* Common compare method for io operations with a user provided tag */
typedef int (*io_compare_cb)(const io_record_t *, const io_record_t *,
                               void *tag);

/* A function which is expected to return 0..num_part-1 based upon the given record and the user provided tag. */
typedef size_t (*io_partition_cb)(const io_record_t *r, size_t num_part,
                                    void *tag);

/* Similar to above, except that instead of using the io_record_t structure, the records are sequential in
   memory each of the fixed record size.  Typically, d will be cast to the fixed record structure. */
typedef bool (*io_fixed_reducer_cb)(char *d, size_t num_r, void *tag);

/* A user provided sort function where p points to the records.  This can speed up sorting by allowing the
   compare function to be inlined (see the-macro-library).  This also saves memory since the io_record_t
   structures are not used.  */
typedef void (*io_fixed_sort_cb)(void *p, size_t total_elems);

/* Similar to io_compare_cb except that p1, p2 point to the record themselves */
typedef int (*io_fixed_compare_cb)(const void *p1, const void *p2, void *tag);

/* This is meant to provide the end user the ability to skip certain files in various operations such as
   directory listing or reading files */
typedef bool (*io_file_valid_cb)(const char *filename, void *arg);

// _macro_sort_compare_h produces the function header following...
_macro_sort_compare_h(io_sort_records, cmp_arg, io_record_t);
// void io_sort_records(io_record_t *base, size_t n, io_compare_cb cmp, void *arg);

/* A commonly used reducer which simply takes the first instance of a record. */
bool io_keep_first(io_record_t *res, const io_record_t *r,
                      size_t num_r, aml_buffer_t *bh, void *tag);

/* A commonly used partition function which can hash a record or the tail of the record and then assign a
   partition.  To hash the whole record, pass NULL as the tag or a size_t value set to zero.  To hash the whole
   record except the first N bytes, pass a pointer to a size_t set to N. */
size_t io_hash_partition(const io_record_t *r, size_t num_part,
                            void *tag);


/* exposes stats about files */
typedef struct io_file_info_s {
  char *filename;
  size_t size;
  time_t last_modified;
  int32_t tag; // defaults to zero
} io_file_info_t;


/* A function which is expected to return 0..num_part-1 based upon the given file_info structure
   and the user provided tag.  Files can be skipped by returning num_part. */
typedef size_t (*io_partition_file_cb)(const io_file_info_t *fi, size_t num_part,
                                       void *tag);

/* get size, last_modified of filename.  filename is expected to be an absolute path. */
bool io_file_info(io_file_info_t *fi);

/* hashes a filename - typically for partitioning. */
uint64_t io_hash_filename(const char *filename);

/* io_list returns an array of io_file_info_t structures for all valid files.  The response is expected to be
   freed using aml_free.  */
#ifdef _AML_DEBUG_
#define io_list(path, num_files, file_valid, arg) io_list_d(path, num_files, file_valid, arg, aml_file_line_func("io_list"))
io_file_info_t *
io_list_d(const char *path, size_t *num_files,
        io_file_valid_cb file_valid, void *arg, const char *caller);
#else
#define io_list(path, num_files, file_valid, arg) io_list_d(path, num_files, file_valid, arg)
io_file_info_t *
io_list_d(const char *path, size_t *num_files,
          io_file_valid_cb file_valid, void *arg);
#endif
/* Similar to io_list except that the response is allocated using a pool which means that the response's memory
   does not have to be reclaimed (or more precisely is managed by the pool) */
io_file_info_t *
io_pool_list(aml_pool_t *pool, const char *path, size_t *num_files,
             io_file_valid_cb file_valid, void *arg);

/* select only file_info structures which match a given partition. */
io_file_info_t *io_partition_file_info(aml_pool_t *pool, size_t *num_res,
                                       io_file_info_t *inputs,
                                       size_t num_inputs,
                                       size_t partition,
                                       size_t num_partitions,
                                       io_partition_file_cb partition_cb,
                                       void *tag);

/* sorts file_info list by last_modified, size, or filename and optionally descending */
void io_sort_file_info_by_last_modified(io_file_info_t *files, size_t num_files);
void io_sort_file_info_by_last_modified_descending(io_file_info_t *files, size_t num_files);
void io_sort_file_info_by_size(io_file_info_t *files, size_t num_files);
void io_sort_file_info_by_size_descending(io_file_info_t *files, size_t num_files);
void io_sort_file_info_by_filename(io_file_info_t *files, size_t num_files);
void io_sort_file_info_by_filename_descending(io_file_info_t *files, size_t num_files);

/* check if a file exists */
bool io_file_exists(const char *filename);

/* get the size of a file */
size_t io_file_size(const char *filename);

/* get the last modified timestamp of a file */
time_t io_modified(const char *filename);

/* is this a directory */
bool io_directory(const char *filename);

/* is this a file */
bool io_file(const char *filename);

/* this will return the full path of the file if it exists by searching
   the current directory and then parent directories. */
char *io_find_file_in_parents(const char *path);

/* Read the contents of filename into a buffer and return it's length.  The
   buffer should be freed using aml_free.

  The following macro is equivalent to
  char *io_read_file(size_t *len, const char *filename);

  If _AML_DEBUG_ is defined and the memory is not freed properly using aml_free, an error will be
  reported when the program exits indicating the memory loss.
*/
#ifdef _AML_DEBUG_
#define io_read_file(len, filename)                                         \
  _io_read_file(len, filename, aml_file_line_func("io_read_file"))
char *_io_read_file(size_t *len, const char *filename, const char *caller);
#else
#define io_read_file(len, filename) _io_read_file(len, filename)
char *_io_read_file(size_t *len, const char *filename);
#endif

#ifdef _AML_DEBUG_
#define io_read_chunk(len, filename, offset, length)                                \
  _io_read_chunk(len, filename, offset, length, aml_file_line_func("io_read_file"))
char *_io_read_chunk(size_t *len, const char *filename, size_t offset, size_t length, const char *caller);
#else
#define io_read_chunk(len, filename) _io_read_file(len, filename, offset, length)
char *_io_read_chunk(size_t *len, const char *filename, size_t offset, size_t length);
#endif


bool io_read_chunk_into_buffer(char *buffer, size_t *len,
                               const char *filename, size_t offset, size_t length);


/* The memory allocated must be freed using free (NOTE THAT THIS IS NOT aml_free for this one case!).
   The size of the file must also be a multiple of alignment. */
char *io_read_file_aligned(size_t *len, size_t alignment, const char *filename);

/* Similar to io_read_file, except the file is allocated using the pool */
char *io_pool_read_file(aml_pool_t *pool, size_t *len, const char *filename);

char *io_pool_read_chunk(aml_pool_t *pool, size_t *len,
                         const char *filename, size_t offset, size_t length);


/*
  Make the given directory if it doesn't already exist.  Return false if an
  error occurred.
*/
bool io_make_directory(const char *path);

/*
   This will take a full filename and temporarily place a \0 in place of the
   last slash (/).  If there isn't a slash, then it will return true. Otherwise,
   it will check to see if path exists and create it if it does not exist.  If
   an error occurs, false will be returned.
*/
bool io_make_path_valid(char *filename);

/*
  test if filename has extension, (ex - "lz4", "" if no extension expected)
  If filename is NULL, false will be returned.
*/
bool io_extension(const char *filename, const char *extension);

/* compare the first 32 or 64 bits of a record */
static inline int io_compare_uint32_t(const io_record_t *p1,
                                         const io_record_t *p2, void *tag __attribute__((unused))) {
  uint32_t *a = (uint32_t *)p1->record;
  uint32_t *b = (uint32_t *)p2->record;
  if (*a != *b)
    return (*a < *b) ? -1 : 1;
  return 0;
}

static inline int io_compare_uint64_t(const io_record_t *p1,
                                         const io_record_t *p2, void *tag __attribute__((unused))) {
  uint64_t *a = (uint64_t *)p1->record;
  uint64_t *b = (uint64_t *)p2->record;
  if (*a != *b)
    return (*a < *b) ? -1 : 1;
  return 0;
}

/* split by the first 32 or 64 bits of a record */
static inline size_t io_split_by_uint32_t(const io_record_t *r,
                                             size_t num_part, void *tag __attribute__((unused))) {
  uint32_t *a = (uint32_t *)r->record;
  uint32_t np = num_part;
  return (*a) % np;
}

static inline size_t io_split_by_uint64_t(const io_record_t *r,
                                             size_t num_part, void *tag __attribute__((unused))) {
  uint64_t *a = (uint64_t *)r->record;
  return (*a) % num_part;
}

/* split by the second 32 or 64 bits of a record */
static inline size_t io_split_by_uint32_t_2(const io_record_t *r,
                                               size_t num_part, void *tag __attribute__((unused))) {
  uint32_t *a = (uint32_t *)r->record;
  uint32_t np = num_part;
  return (a[1]) % np;
}

static inline size_t io_split_by_uint64_t_2(const io_record_t *r,
                                               size_t num_part, void *tag __attribute__((unused))) {
  uint64_t *a = (uint64_t *)r->record;
  return (a[1]) % num_part;
}

#ifdef __cplusplus
}
#endif

#endif
