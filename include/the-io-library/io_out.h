// SPDX-FileCopyrightText: 2019–2025 Andy Curtis <contactandyc@gmail.com>
// SPDX-FileCopyrightText: 2024–2025 Knode.ai — technical questions: contact Andy (above)
// SPDX-License-Identifier: Apache-2.0

#ifndef _io_out_H
#define _io_out_H


#include "a-memory-library/aml_alloc.h"
#include "the-io-library/io.h"
#include "the-io-library/io_in.h"
#include "the-lz4-library/lz4.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "the-io-library/src/io_out.h"

/* open a file for writing with a given filename */
io_out_t *io_out_init(const char *filename, io_out_options_t *options);

/* use an open file for writing.  If fd_owner is true, the file descriptor
   will be closed when this is destroyed. */
io_out_t *io_out_init_with_fd(int fd, bool fd_owner, io_out_options_t *options);

/* write to a regular, partitioned, or sorted file. */
io_out_t *io_out_ext_init(const char *filename, io_out_options_t *options,
                          io_out_ext_options_t *ext_options);

/* write record in the format specified by io_out_options_format(...) */
bool io_out_write_record(io_out_t *h, const void *d, size_t len);

/* This only works if output is sorted.  This will bypass the writing of the
   final file and give you access to the cursor. */
io_in_t *io_out_in(io_out_t *h);

/* destroy the output. */
void io_out_destroy(io_out_t *h);

/* these methods only work if writing to a single file */
bool io_out_write(io_out_t *h, const void *d, size_t len);
bool io_out_write_prefix(io_out_t *h, const void *d, size_t len);
bool io_out_write_delimiter(io_out_t *h, const void *d, size_t len,
                            char delimiter);

/* io_out_options_t is declared in src/io_out.h.  h is expected to point to a
   structure of this type (and not NULL). */
void io_out_options_init(io_out_options_t *h);

/* set the buffer size that the io_out handle has to use. */
void io_out_options_buffer_size(io_out_options_t *h, size_t buffer_size);

/* This should be called with one of the io_format... methods.

   Prefix format (4 byte length prefix before each record),
   io_in_options_format(&options, io_prefix());

   Delimiter format (specify a character at the end of a record)
   io_in_options_format(&options, io_delimiter('\n'));

   Fixed format (all records are the same length)
   io_in_options_format(&options, io_fixed(<some_length>));

   Other formats may be added in the future such as compressed, protobuf, etc.
*/
void io_out_options_format(io_out_options_t *h, io_format_t format);

void io_out_options_abort_on_error(io_out_options_t *h);

/*
  Open the file in append mode.  This currently does not work for lz4 files.
*/
void io_out_options_append_mode(io_out_options_t *h);

/*
  Write the file with a -safe name and rename it upon completion.
*/
void io_out_options_safe_mode(io_out_options_t *h);

/*
  Write an ack file after the file has been closed.  This is useful if another
  program is picking up that the file is finished.
*/
void io_out_options_write_ack_file(io_out_options_t *h);

/*
  Set the level of compression and identify the output as gzip if filename
  is not present.
*/
void io_out_options_gz(io_out_options_t *h, int level);

/*
  Set the level of compression, the block size, whether block checksums are
  used, and content checksums.  The default is that the checksums are not
  used and block size is s64kb.  level defaults to 1.
*/
void io_out_options_lz4(io_out_options_t *h, int level,
                        lz4_block_size_t size, bool block_checksum,
                        bool content_checksum);

/* extended options are for partitioned output, sorted output, or both */
void io_out_ext_options_init(io_out_ext_options_t *h);

/* Normally if data is partitioned and sorted, partitioning would happen first.
   This has the added cost of writing the unsorted partitions.  Because the
   data is partitioned first, sorting can happen in parallel.  In some cases,
   it may be desirable to sort first and then partition the sorted data.  This
   option exists for those cases. */
void io_out_ext_options_sort_before_partitioning(io_out_ext_options_t *h);

/* Normally sorting will happen after partitions are written as more threads
   can be used for doing this.  However, sorting can occur while the partitions
   are being written out using this option. */
void io_out_ext_options_sort_while_partitioning(io_out_ext_options_t *h);

/* when partitioning and sorting - how many partitions can be sorted at once? */
void io_out_ext_options_num_sort_threads(io_out_ext_options_t *h,
                                         size_t num_sort_threads);

/* options for creating a partitioned output */
void io_out_ext_options_partition(io_out_ext_options_t *h,
                                  io_partition_cb part, void *arg);

void io_out_ext_options_num_partitions(io_out_ext_options_t *h,
                                       size_t num_partitions);

/* By default, tmp files are written every time the buffer fills and all of the
   tmp files are merged at the end.  This causes the tmp files to be merged
   once the number of tmp files reaches the num_per_group. */
void io_out_ext_options_intermediate_group_size(io_out_ext_options_t *h,
                                                size_t num_per_group);

/* options for comparing output */
void io_out_ext_options_compare(io_out_ext_options_t *h,
                                io_compare_cb compare, void *arg);

void io_out_ext_options_intermediate_compare(io_out_ext_options_t *h,
                                             io_compare_cb compare,
                                             void *arg);

/* set the reducers */
void io_out_ext_options_reducer(io_out_ext_options_t *h,
                                io_reducer_cb reducer, void *arg);

/* Overrides the reducer specified by io_out_ext_options_reducer for
   internal reducing of file. */
void io_out_ext_options_intermediate_reducer(io_out_ext_options_t *h,
                                             io_reducer_cb reducer,
                                             void *arg);

/* Use an extra thread when sorting output. */
void io_out_ext_options_use_extra_thread(io_out_ext_options_t *h);

/* Default tmp files are stored in lz4 format.  Disable this behavior. */
void io_out_ext_options_dont_compress_tmp(io_out_ext_options_t *h);

/* used to create a partitioned filename */
void io_out_partition_filename(char *dest, const char *filename, size_t id);

#ifdef __cplusplus
}
#endif

#endif
