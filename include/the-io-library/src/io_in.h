// SPDX-FileCopyrightText: 2019–2025 Andy Curtis <contactandyc@gmail.com>
// SPDX-FileCopyrightText: 2024–2025 Knode.ai — technical questions: contact Andy (above)
// SPDX-License-Identifier: Apache-2.0

struct io_in_s;
typedef struct io_in_s io_in_t;
struct io_out_s;
typedef struct io_out_s io_out_t;

typedef struct {
  size_t buffer_size;
  size_t compressed_buffer_size;

  io_format_t format;
  bool abort_on_error;
  bool abort_on_partial_record;
  bool abort_on_file_not_found;
  bool abort_on_file_empty;
  int32_t tag;

  bool gz;
  bool lz4;

  bool full_record_required;

  io_compare_cb compare;
  void *compare_arg;
  io_reducer_cb reducer;
  void *reducer_arg;
} io_in_options_t;

void _io_in_empty(io_in_t *h);
