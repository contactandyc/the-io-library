// SPDX-FileCopyrightText: 2019–2025 Andy Curtis <contactandyc@gmail.com>
// SPDX-FileCopyrightText: 2024–2025 Knode.ai — technical questions: contact Andy (above)
// SPDX-License-Identifier: Apache-2.0

typedef struct {
  size_t buffer_size;
  bool append_mode;
  bool safe_mode;
  bool write_ack_file;
  bool abort_on_error;
  io_format_t format;

  int level;
  lz4_block_size_t size;
  bool block_checksum;
  bool content_checksum;

  bool gz;
  bool lz4;
} io_out_options_t;

typedef struct {
  /* need to set first block */
  bool use_extra_thread;
  bool lz4_tmp;

  bool sort_before_partitioning;
  bool sort_while_partitioning;
  size_t num_sort_threads;

  io_partition_cb partition;
  void *partition_arg;
  size_t num_partitions;

  io_compare_cb compare;
  void *compare_arg;

  size_t num_per_group;
  io_compare_cb int_compare;
  void *int_compare_arg;

  io_reducer_cb reducer;
  void *reducer_arg;

  io_reducer_cb int_reducer;
  void *int_reducer_arg;

  io_fixed_reducer_cb fixed_reducer;
  void *fixed_reducer_arg;

  io_fixed_compare_cb fixed_compare;
  void *fixed_compare_arg;

  io_fixed_sort_cb fixed_sort;
  void *fixed_sort_arg;
} io_out_ext_options_t;
