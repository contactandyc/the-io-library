// SPDX-FileCopyrightText: 2019–2025 Andy Curtis <contactandyc@gmail.com>
// SPDX-FileCopyrightText: 2024–2025 Knode.ai — technical questions: contact Andy (above)
// SPDX-License-Identifier: Apache-2.0

struct io_in_base_s;
typedef struct io_in_base_s io_in_base_t;

typedef struct {
  char *buffer;
  size_t used;
  size_t size;
  size_t pos;
  bool eof;
  bool can_free;
} io_in_buffer_t;
