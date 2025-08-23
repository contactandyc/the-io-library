// SPDX-FileCopyrightText: 2019–2025 Andy Curtis <contactandyc@gmail.com>
// SPDX-FileCopyrightText: 2024–2025 Knode.ai — technical questions: contact Andy (above)
// SPDX-License-Identifier: Apache-2.0

#ifndef _io_data_store_H
#define _io_data_store_H

#include "a-memory-library/aml_pool.h"
#include <stdint.h>

/* Filenames are limited to ~200 characters including the store's base path. */
struct io_data_store_s;
typedef struct io_data_store_s io_data_store_t;

struct io_data_store_cursor_s;
typedef struct io_data_store_cursor_s io_data_store_cursor_t;

io_data_store_t *io_data_store_init(const char *path);

bool io_data_store_exists(io_data_store_t *h, const char *filename);

char *io_data_store_read_file(io_data_store_t *h, size_t *file_length, const char *filename);

char *io_data_store_pool_read_file(io_data_store_t *h, aml_pool_t *pool, size_t *file_length, const char *filename);

/* The temp_id is used to write the file before renaming it to the final name. */
void io_data_store_write_file(io_data_store_t *h, const char *filename, const char *data, size_t data_length,
                              uint32_t temp_id);

void io_data_store_remove_file(io_data_store_t *h, const char *filename);

io_data_store_cursor_t *io_data_store_cursor_init(io_data_store_t *h);
bool io_data_store_cursor_next(io_data_store_cursor_t *cursor, char **filename, char **data, size_t *file_length);

void io_data_store_destroy(io_data_store_t *h);

#endif
