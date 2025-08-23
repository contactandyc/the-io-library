// SPDX-FileCopyrightText: 2019–2025 Andy Curtis <contactandyc@gmail.com>
// SPDX-FileCopyrightText: 2024–2025 Knode.ai — technical questions: contact Andy (above)
// SPDX-License-Identifier: Apache-2.0

#ifndef _io_in_base_H
#define _io_in_base_H

#include "a-memory-library/aml_alloc.h"

#include <inttypes.h>
#include <sys/types.h>

/*
   This is generally meant to be an internal object, but can be used
   externally if a need arises.  io_in should be used instead.

   This should not be documented for the website usage.
*/
#ifdef __cplusplus
extern "C" {
#endif

#include "the-io-library/src/io_in_base.h"

io_in_base_t *io_in_base_init_gz(const char *filename, int fd, bool can_close,
                                 size_t buffer_size);
io_in_base_t *io_in_base_init(const char *filename, int fd, bool can_close,
                              size_t buffer_size);
io_in_base_t *io_in_base_init_from_buffer(char *buffer, size_t buffer_size,
                                          bool can_free);
io_in_base_t *io_in_base_reinit(io_in_base_t *base, size_t buffer_size);

const char *io_in_base_filename(io_in_base_t *h);

char *io_in_base_read_delimited(io_in_base_t *h, int32_t *rlen, int delim,
                                bool required);

/*
  returns NULL if len bytes not available
*/
char *io_in_base_read(io_in_base_t *h, int32_t len);

/*
  places a zero after *rlen bytes
*/
char *io_in_base_readz(io_in_base_t *h, int32_t *rlen, int32_t len);

void io_in_base_destroy(io_in_base_t *h);

#ifdef __cplusplus
}
#endif


#endif
