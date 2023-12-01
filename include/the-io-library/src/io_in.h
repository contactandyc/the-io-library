/*
Copyright 2019 Andy Curtis

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

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
