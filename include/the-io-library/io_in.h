// SPDX-FileCopyrightText: 2019–2025 Andy Curtis <contactandyc@gmail.com>
// SPDX-FileCopyrightText: 2024–2025 Knode.ai — technical questions: contact Andy (above)
// SPDX-License-Identifier: Apache-2.0

#ifndef _io_in_H
#define _io_in_H

/* The io_in object is meant for reading input (generally files, but file
   descriptors, sets of records, and blocks of memory can also be used).  The
   object can be configured as a single input or a set of inputs and then act
   as a single input.  This allows functions to be written that can operate
   on a variety of inputs (one file, many files, files and buffers mixed, lists
   of files, etc).  Sets of sorted files where each file is sorted can be
   joined together as a single unit and remain sorted.  If the sorted files
   are distinct internally, but not within the set, the join can be configured
   to reduce the non-distinct elements.

   Another useful feature supported by io_in is built in compression.  Files
   with an extension of .gz or .lz4 are automatically read from their
   respective formats without the need to first decompress the files.

   In general, the io_in object is meant to iterate over one or more files of
   records where the records are prefix formatted, delimiter formatted, or
   fixed length.  Additional formats can be added as needed, but these three
   cover most use cases.  The prefix format prefixes a 4 byte length at the
   beginning of each record.  The delimiter format expects a delimiter such as
   a newline.  The fixed length format expects all records to be a similar
   size.

   Because disks are slow and RAM is fast, buffering is built into the io_in
   object.  A buffer size can be defined to reduce IO (particularly seeks).

   The io_in object plays an important role in sorting files.  When large
   files are sorted, tmp files are written to disk which are themselves
   sorted.  To finally sort the data, the tmp files are joined and further
   reduced if necessary as an io_in object.  The io_out object allows one
   to call io_out_in which can be used to avoid writing the final sort file
   if desired.  Chaining sorts benefits from this in that the tmp files can
   be piped directly into the next sort.

   Because the io_in (and the io_out) objects can be configured, it makes it
   possible to define pipelines of data with very little extra code.  The
   map_reduce object in map-reduce-library heavily uses the io_in and io_out
   objects to allow for very large data transformations in a defined environment.
*/


#include "a-memory-library/aml_alloc.h"
#include "the-io-library/io_in_base.h"
#include "the-io-library/io.h"
#include "the-lz4-library/lz4.h"

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "the-io-library/src/io_in.h"

typedef io_in_t *(*io_in_init_cb)( void * );

typedef void (*io_in_transform_cb)(io_in_t *in, io_out_t *out, void *arg);

/* io_in_options_t is declared in ac-io/io_in.h and not opaque.  h is expected
   to point to a structure of this type (and not NULL). */
void io_in_options_init(io_in_options_t *h);

/* Set the buffer size for reading input from files.  If the input is
   compressed, the buffer_size here is for the uncompressed content.
   Ideally, this would be large enough to support any record in the input.
   If an individual record is larger than the buffer_size, a temporary
   buffer will be created to hold the given record.  The temporary buffer
   should happen as the exception (if at all). */
void io_in_options_buffer_size(io_in_options_t *h, size_t buffer_size);

/* This should be called with one of the io_format... methods.

   Prefix format (4 byte length prefix before each record),
   io_in_options_format(&options, io_prefix());

   Delimiter format (specify a character at the end of a record)
   io_in_options_format(&options, io_delimiter('\n'));

   Fixed format (all records are the same length)
   io_in_options_format(&options, io_fixed(<some_length>));

   Other formats may be added in the future such as compressed, protobuf, etc.
*/
void io_in_options_format(io_in_options_t *h, io_format_t format);

/* This generally applies to opening compressed files which have a corrupt
   format.  If the format of the file is corrupt, abort() will be called
   instead of prematurely ending the file. */
void io_in_options_abort_on_error(io_in_options_t *h);

/* If a file has an incomplete record at the end, the record will be dropped
   unless it this is set. */
void io_in_options_allow_partial_records(io_in_options_t *h);

/* If a partial record exists at the end of a file, the record would normally
   be silently dropped (unless partial records are allowed above).  Setting
   this would cause the program to abort on a partial record. */
void io_in_options_abort_on_partial(io_in_options_t *h);

/* If a file is not found, abort (instead of treating it as an empty file). */
void io_in_options_abort_on_file_not_found(io_in_options_t *h);

/* If a file is empty, abort() */
void io_in_options_abort_on_file_empty(io_in_options_t *h);

/* This tag can be useful to distinguish one file from another.  It can also
   be used as follows...

   io_in_options_tag(&options, <some_int>);
   io_in_add(h, in, options.tag);
*/
void io_in_options_tag(io_in_options_t *h, int tag);

/* Indicate that the contents are compressed (if using a file descriptor or
   buffer).  filenames are determined to be compressed if they end in .gz
   or .lz4.  The buffer_size is the size to buffer compressed content which
   will default to buffer_size. */
void io_in_options_gz(io_in_options_t *h, size_t buffer_size);
void io_in_options_lz4(io_in_options_t *h, size_t buffer_size);
void io_in_options_compressed_buffer_size(io_in_options_t *h,
                                          size_t buffer_size);

/* Within a single cursor, reduce equal items.  In this case, it is assumed
   that the contents are sorted.  */
void io_in_options_reducer(io_in_options_t *h, io_compare_cb compare,
                           void *compare_arg, io_reducer_cb reducer,
                           void *reducer_arg);

/* The filename dictates whether the file is normal, gzip compressed (.gz
   extension), or lz4 compressed (.lz4 extension).  If options is NULL, default
   options will be used. NULL will be returned if the file cannot be opened.
*/
io_in_t *io_in_init(const char *filename, io_in_options_t *options);

/* This is just like io_in_init, except that it sets the format and buffer_size
   using parameters instead of requiring the io_in_options_t structure.  It is
   meant to be a helper function which reduces required code for this use case.
*/
io_in_t *io_in_quick_init(const char *filename, io_format_t format, size_t buffer_size);

/* Uses the file descriptor to create the input stream.  options dictate the
   compression state of input.  can_close should normally be true meaning that
   when the io_in object is destroyed, the file should be closed.
*/
io_in_t *io_in_init_with_fd(int fd, bool can_close, io_in_options_t *options);

/* This creates a stream from a buffer. options dictate the compression state
   of input.  can_free should normally be true meaning that when the io_in
   object is destroyed, the buffer should be freed. */
io_in_t *io_in_init_with_buffer(void *buf, size_t len, bool can_free,
                                io_in_options_t *options);

/* Use this to create an io_in_t which allows cursoring over an array of
   io_record_t structures. */
io_in_t *io_in_records_init(io_record_t *records, size_t num_records,
                            io_in_options_t *options);

/* Use this function to create an io_in_t which allows multiple input streams */
io_in_t *io_in_ext_init(io_compare_cb compare, void *arg,
                        io_in_options_t *options);

/* Create an input which will open sequentially until cb returns NULL */
io_in_t *io_in_init_from_cb(io_in_init_cb cb, void *arg);

/* Create an input which will open one file at a time in files */
io_in_t *io_in_init_from_list(io_file_info_t *files, size_t num_files,
                              io_in_options_t *options);

/* Create an empty cursor - still must be destroyed and won't return anything */
io_in_t *io_in_empty(void);

/* Useful to limit the number of records for testing */
void io_in_limit(io_in_t *h, size_t limit);

/* After in is destroyed, destroy the given output, useful in transformations */
void io_in_destroy_out(io_in_t *in, io_out_t *out,
                       void (*destroy_out)(io_out_t *out));

/* Transform the data and return a new cursor */
io_in_t *io_in_transform(io_in_t *in, io_format_t format, size_t buffer_size,
                         io_compare_cb compare, void *compare_arg,
                         io_reducer_cb reducer, void *reducer_arg,
                         io_in_transform_cb transform, void *arg);

/* When there are multiple input streams, this would keep only the first equal
   record across the streams. */
void io_in_ext_keep_first(io_in_t *h);

/* When there are multiple input streams, set the reducer */
void io_in_ext_reducer(io_in_t *h, io_reducer_cb reducer, void *arg);

/* The tag can be options->tag from init of in if that makes sense.  Otherwise,
  this can be useful to distinguish different input sources. The first param
  h must be initialized with io_in_init_compare. */
void io_in_ext_add(io_in_t *h, io_in_t *in, int tag);

/* Count the records and close the cursor */
size_t io_in_count(io_in_t *h);

/* Advance to the next record and return it. */
io_record_t *io_in_advance(io_in_t *h);

/* Get the current record (this will be NULL if advance hasn't been called or
 * io_in_reset was called). */
io_record_t *io_in_current(io_in_t *h);

/* Make the next call to advance return the same record as the current.  This is
  particularly helpful when advancing in a loop until a given record. */
void io_in_reset(io_in_t *h);

/* Return the next equal record across all of the streams.  num_r will be the
   number of streams containing the next record.  It is assumed that each
   stream will have exactly one equal record. If there is only one stream,
   num_r will always be 1 until the stream is finished. */
io_record_t *io_in_advance_unique(io_in_t *h, size_t *num_r);

/* Given the comparison function and arg, get all records which are equal and
   return them as an array.  This will work with one or more input streams. */
io_record_t *io_in_advance_group(io_in_t *h, size_t *num_r,
                                    bool *more_records, io_compare_cb compare,
                                    void *arg);

/* Destroy the input stream (or set of input streams) */
void io_in_destroy(io_in_t *h);

/* These are experimental and don't need documented yet */

typedef void (*io_in_out_cb)(io_out_t *out, io_record_t *r, void *arg);
typedef void (*io_in_out2_cb)(io_out_t *out, io_out_t *out2, io_record_t *r,
                             void *arg);
typedef void (*io_in_out_group_cb)(io_out_t *out, io_record_t *r,
                                  size_t num_r, bool more_records, void *arg);
typedef void (*io_in_out_group2_cb)(io_out_t *out, io_out_t *out2,
                                   io_record_t *r, size_t num_r,
                                   bool more_records, void *arg);

/* These are very common transformations.  All of these take a single input and
   write to one or two outputs.  The custom functions allow for custom filtering
   and various other transformations.  The group functions will group records
   based upon the compare function.  */

/* Write all records from in to out */
void io_in_out(io_in_t *in, io_out_t *out);

/* Call custom callback for every input record (with out, record, and arg). */
void io_in_out_custom(io_in_t *in, io_out_t *out, io_in_out_cb cb, void *arg);

/* Write all records from in to both out and out2 */
void io_in_out2(io_in_t *in, io_out_t *out, io_out_t *out2);

/* Call custom callback for every input record (with out, out2, record, and
   arg). */
void io_in_out_custom2(io_in_t *in, io_out_t *out, io_out_t *out2,
                       io_in_out2_cb cb, void *arg);

void io_in_out_group(io_in_t *in, io_out_t *out, io_compare_cb compare,
                     void *compare_arg, io_in_out_group_cb group, void *arg);

void io_in_out_group2(io_in_t *in, io_out_t *out, io_out_t *out2,
                      io_compare_cb compare, void *compare_arg,
                      io_in_out_group2_cb group, void *arg);

#ifdef __cplusplus
}
#endif

#endif
