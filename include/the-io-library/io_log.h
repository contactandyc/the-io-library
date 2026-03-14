// SPDX-FileCopyrightText: 2019–2026 Andy Curtis <contactandyc@gmail.com>
// SPDX-FileCopyrightText: 2024–2025 Knode.ai — technical questions: contact Andy (above)
// SPDX-License-Identifier: Apache-2.0

#ifndef _io_log_H
#define _io_log_H

#include "the-io-library/io_in.h"
#include "the-io-library/io_out.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct io_log_s io_log_t;

/* -------------------------------------------------------------------------
 * Build sequential io_in cursor over all logs.
 * include_active == true → append <base>.active at end.
 * ------------------------------------------------------------------------- */
io_in_t *io_log_in(const char *base, bool include_active);

/* -------------------------------------------------------------------------
 * Create a crash-safe rotating log writer.
 *
 *  - base: path to base file, e.g. "/var/log/app.log"
 *          Active file is "<dir>/<base>.active"
 *  - opts: io_out options. Compression applies to rotated files only.
 *  - rotate_size: rotate after N bytes (0 = disabled)
 *  - rotate_interval: rotate after N seconds (0 = disabled)
 *  - max_files: keep at most this many compressed rotated logs (0 = keep all)
 *
 *  NOTE:
 *    * All logs use prefix format (format = 0).
 *    * Each record written uses the new structured header below.
 * -------------------------------------------------------------------------
 * Record layout inside each prefix record:
 *
 *   [uint64_t timestamp]         (8 bytes)
 *   [uint32_t hash_with_flag]    (4 bytes)
 *     - Bits 31..1 = XXH32 hash of remaining payload (after hash field)
 *     - Bit 0      = TTL flag (1 if TTL field follows)
 *   [uint32_t ttl_seconds]?      (4 bytes, optional)
 *   [payload bytes...]
 *
 * The hash is computed over everything after the hash field:
 *   → [ttl?][payload...]
 * The low bit of the stored hash indicates whether TTL was present.
 * ------------------------------------------------------------------------- */
io_log_t *io_log_init(const char *base,
                      io_out_options_t *opts,
                      size_t rotate_size,
                      time_t rotate_interval,
                      size_t max_files);

/* -------------------------------------------------------------------------
 * Writing API
 * ------------------------------------------------------------------------- */

/* Write a record using current time (no TTL) */
bool io_log_write(io_log_t *h, const void *data, size_t len);

/* Write a record with explicit timestamp (no TTL) */
bool io_log_write_ts(io_log_t *h, uint64_t ts_sec, const void *data, size_t len);

/* Write a record with explicit timestamp and TTL (seconds) */
bool io_log_write_ts_ttl(io_log_t *h, uint64_t ts_sec, uint32_t ttl_sec,
                         const void *data, size_t len);

/* Flush & close current active file; a later write will reopen it. */
void io_log_flush(io_log_t *h);

/* Close and free. Compression threads may continue detached. */
void io_log_destroy(io_log_t *h);

#ifdef __cplusplus
}
#endif
#endif
