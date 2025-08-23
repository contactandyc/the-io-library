// SPDX-FileCopyrightText: 2019–2025 Andy Curtis <contactandyc@gmail.com>
// SPDX-FileCopyrightText: 2024–2025 Knode.ai — technical questions: contact Andy (above)
// SPDX-License-Identifier: Apache-2.0

#include "the-io-library/io_in.h"

#include "the-lz4-library/lz4.h"
#include "a-memory-library/aml_buffer.h"
#include "a-memory-library/aml_alloc.h"

#include "the-io-library/io_out.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>

void io_out_destroy(io_out_t *out);

typedef io_record_t *(*io_in_advance_cb)(io_in_t *h);
typedef io_record_t *(*io_in_advance_unique_cb)(io_in_t *h, size_t *num_r);

static const int IO_IN_NORMAL_TYPE = 0; // default (due to memset)
static const int IO_IN_EXT_TYPE = 1;
static const int IO_IN_RECORDS_TYPE = 2;
static const int IO_IN_LIST_TYPE = 3;
static const int IO_IN_CB_TYPE = 4;

size_t io_in_count(io_in_t *h) {
  if(!h)
    return 0;
  size_t r = 0;
  while (io_in_advance(h))
    r++;
  io_in_destroy(h);
  return r;
}

struct io_in_cb_s;
typedef struct io_in_cb_s io_in_cb_t;

struct io_in_cb_s {
  io_in_options_t options;
  int type;
  io_record_t rec;
  io_record_t *current;
  size_t num_current;
  io_record_t *current_tmp;
  size_t num_current_tmp;
  io_in_advance_cb advance;
  io_in_advance_cb advance_tmp;
  io_in_advance_unique_cb advance_unique;
  io_in_advance_unique_cb advance_unique_tmp;
  io_in_advance_cb count_advance;
  size_t limit;
  size_t record_num;
  io_out_t *out;
  void (*destroy_out)(io_out_t *out);
  aml_buffer_t *group_bh;

  io_in_init_cb cb;
  void *arg;
  io_in_t *cur_in;
};


struct io_in_list_s;
typedef struct io_in_list_s io_in_list_t;

struct io_in_list_s {
  io_in_options_t options;
  int type;
  io_record_t rec;
  io_record_t *current;
  size_t num_current;
  io_record_t *current_tmp;
  size_t num_current_tmp;
  io_in_advance_cb advance;
  io_in_advance_cb advance_tmp;
  io_in_advance_unique_cb advance_unique;
  io_in_advance_unique_cb advance_unique_tmp;
  io_in_advance_cb count_advance;
  size_t limit;
  size_t record_num;
  io_out_t *out;
  void (*destroy_out)(io_out_t *out);
  aml_buffer_t *group_bh;

  io_file_info_t *file_list;
  io_file_info_t *filep;
  io_file_info_t *fileep;
  io_in_t *cur_in;
};

struct io_in_s {
  io_in_options_t options;
  int type;
  io_record_t rec;
  io_record_t *current;
  size_t num_current;
  io_record_t *current_tmp;
  size_t num_current_tmp;
  io_in_advance_cb advance;
  io_in_advance_cb advance_tmp;
  io_in_advance_unique_cb advance_unique;
  io_in_advance_unique_cb advance_unique_tmp;
  io_in_advance_cb count_advance;
  size_t limit;
  size_t record_num;
  io_out_t *out;
  void (*destroy_out)(io_out_t *out);
  aml_buffer_t *group_bh;

  io_in_advance_cb sub_advance;
  aml_buffer_t *reducer_bh;
  aml_buffer_t *reducer_group_bh;

  io_in_base_t *base;

  int delimiter;
  uint32_t fixed;

  lz4_t *lz4;
  aml_buffer_t *bh; // for overflow
  io_in_buffer_t buf;
  uint32_t block_size;
  uint32_t block_header_size;
  char *zerop;
  char zero;
};

io_record_t *io_in_advance_unique_single(io_in_t *h, size_t *num_r) {
  if (!h) {
    *num_r = 0;
    return NULL;
  }
  io_record_t *r = h->advance(h);
  *num_r = h->num_current;
  return r;
}

io_record_t *io_in_advance_group(io_in_t *h, size_t *num_r,
                                    bool *more_records, io_compare_cb compare,
                                    void *arg) {
  // TODO: group within buffer only - detect buffer switch (save first record
  // for comparison).
  *more_records = false;
  if (!h) {
    *num_r = 0;
    return NULL;
  }

  if (!h->group_bh)
    h->group_bh = aml_buffer_init(4096);

  aml_buffer_t *bh = h->group_bh;
  aml_buffer_clear(bh);

  *num_r = 0;

  io_record_t *r;
  if ((r = h->advance(h)) == NULL)
    return NULL;

  aml_buffer_set(bh, &(r->length), sizeof(r->length));
  aml_buffer_append(bh, &(r->tag), sizeof(r->tag));
  aml_buffer_append(bh, r->record, r->length);
  aml_buffer_appendc(bh, 0);
  size_t num_records = 1;

  io_record_t r1;
  r1 = *r;
  r1.record = aml_buffer_data(bh) + sizeof(r->length) + sizeof(r->tag);
  while ((r = h->advance(h)) != NULL) {
    char *b = aml_buffer_data(bh);
    b += sizeof(r->length) + sizeof(r->tag);
    r1.record = b;

    if (compare(&r1, r, arg)) {
      io_in_reset(h);
      break;
    }
    aml_buffer_append(bh, &(r->length), sizeof(r->length));
    aml_buffer_append(bh, &(r->tag), sizeof(r->tag));
    aml_buffer_append(bh, r->record, r->length);
    aml_buffer_appendc(bh, 0);
    r1.record = aml_buffer_data(bh) + sizeof(r->length) + sizeof(r->tag);

    num_records++;
  }
  size_t len = aml_buffer_length(bh);
  io_record_t *res = (io_record_t *)aml_buffer_append_alloc(
      bh, num_records * sizeof(io_record_t));
  io_record_t *rp = res;
  char *ptr = aml_buffer_data(bh);
  char *endp = ptr + len;
  while (ptr < endp) {
    rp->length = (*(uint32_t *)(ptr));
    ptr += sizeof(uint32_t);
    rp->tag = (*(int32_t *)(ptr));
    ptr += sizeof(int32_t);
    rp->record = ptr;
    ptr += rp->length;
    ptr++;
    rp++;
  }
  // h->num_current = num_records;
  // h->current = res;
  *num_r = num_records;
  return res;
}

io_record_t *io_in_advance_unique(io_in_t *h, size_t *num_r) {
  if (!h) {
    *num_r = 0;
    return NULL;
  }
  return h->advance_unique(h, num_r);
}

io_record_t *io_in_current(io_in_t *h) {
  if (!h)
    return NULL;
  return h->current;
}

io_record_t *io_in_advance(io_in_t *h) {
  if (!h)
    return NULL;

  return h->advance(h);
}

io_record_t *_advance_prefix(io_in_t *h) {
  h->num_current = 1;
  char *p = io_in_base_read(h->base, 4);
  if (!p) {
    h->current = NULL;
    h->num_current = 0;
    return NULL;
  }
  uint32_t length = (*(uint32_t *)p);
  if (length > 0) {
    int32_t len = 0;
    p = io_in_base_readz(h->base, &len, length);
    if (len != length) {
      h->current = NULL;
      h->num_current = 0;
      return NULL;
    }
  }
  h->rec.length = length;
  h->rec.record = p;
  h->current = &(h->rec);
  return h->current;
}

static inline io_record_t *_advance_fixed_(io_in_t *h, uint32_t length) {
  int32_t len = 0;
  h->num_current = 1;
  char *p = io_in_base_readz(h->base, &len, length);
  if (len != length) {
    h->current = NULL;
    h->num_current = 0;
    return NULL;
  }
  h->rec.length = length;
  h->rec.record = p;
  h->current = &(h->rec);
  return h->current;
}

io_record_t *_advance_fixed(io_in_t *h) {
  return _advance_fixed_(h, h->fixed);
}

io_record_t *empty_record(io_in_t *h) { return NULL; }

io_record_t *empty_record_unique(io_in_t *h, size_t *len) {
  *len = 0;
  return NULL;
}

static inline io_record_t *_advance_delimited_(io_in_t *h, int delimiter,
                                                  bool full_record_required) {
  int32_t len = 0;
  char *p =
      io_in_base_read_delimited(h->base, &len, delimiter, full_record_required);
  if (!p) {
    h->current = NULL;
    h->num_current = 0;
    return NULL;
  }
  h->rec.length = len;
  h->rec.record = p;
  h->current = &(h->rec);
  h->num_current = 1;
  return h->current;
}

io_record_t *_advance_delimited(io_in_t *h) {
  return _advance_delimited_(h, h->delimiter, h->options.full_record_required);
}

char *io_in_lz4_read(io_in_t *h, uint32_t len);
char *io_in_lz4_readz(io_in_t *h, int32_t *rlen, uint32_t len);
char *io_in_lz4_read_delimited(io_in_t *h, int32_t *rlen, int delim,
                               bool required);

io_record_t *_advance_prefix_lz4(io_in_t *h) {
  char *p = io_in_lz4_read(h, 4);
  if (!p) {
    h->current = NULL;
    h->num_current = 0;
    return NULL;
  }
  uint32_t length = (*(uint32_t *)p);
  if (length > 0) {
    int32_t len = 0;
    p = io_in_lz4_readz(h, &len, length);
    if (len != length) {
      h->current = NULL;
      h->num_current = 0;
      return NULL;
    }
  }
  h->rec.length = length;
  h->rec.record = p;
  h->current = &(h->rec);
  h->num_current = 1;
  return h->current;
}

static inline io_record_t *_advance_fixed_lz4_(io_in_t *h, uint32_t length) {
  int32_t len = 0;
  char *p = io_in_lz4_readz(h, &len, length);
  if (len != length) {
    h->current = NULL;
    h->num_current = 0;
    return NULL;
  }
  h->rec.length = length;
  h->rec.record = p;
  h->current = &(h->rec);
  h->num_current = 1;
  return h->current;
}

io_record_t *_advance_fixed_lz4(io_in_t *h) {
  return _advance_fixed_lz4_(h, h->fixed);
}

static inline io_record_t *
_advance_delimited_lz4_(io_in_t *h, int delimiter, bool full_record_required) {
  int32_t len = 0;
  char *p = io_in_lz4_read_delimited(h, &len, delimiter, full_record_required);
  if (!p) {
    h->current = NULL;
    h->num_current = 0;
    return NULL;
  }
  h->rec.length = len;
  h->rec.record = p;
  h->current = &(h->rec);
  h->num_current = 1;
  return h->current;
}

io_record_t *_advance_delimited_lz4(io_in_t *h) {
  return _advance_delimited_lz4_(h, h->delimiter,
                                 h->options.full_record_required);
}

static inline void post_reset(io_in_t *h) {
  h->current = h->current_tmp;
  h->num_current = h->num_current_tmp;
  h->current_tmp = NULL;
  h->num_current_tmp = 0;
  h->advance = h->advance_tmp;
  h->advance_unique = h->advance_unique_tmp;
}

static io_record_t *_reset_(io_in_t *h) {
  post_reset(h);
  return h->current;
}

static io_record_t *_reset_unique_(io_in_t *h, size_t *num_r) {
  post_reset(h);
  *num_r = h->num_current;
  return h->current;
}

void io_in_reset(io_in_t *h) {
  if (h->current) {
    h->advance = _reset_;
    h->advance_unique = _reset_unique_;
    h->current_tmp = h->current;
    h->current = NULL;
    h->num_current_tmp = h->num_current;
    h->num_current = 0;
  }
}

void io_in_ext_destroy(io_in_t *h);
void io_in_records_destroy(io_in_t *hp);
void io_in_destroy_from_list(io_in_t *hp);
void io_in_destroy_from_cb(io_in_t *hp);

void io_in_destroy(io_in_t *h) {
  if (!h)
    return;
  if (h->type == IO_IN_EXT_TYPE)
    io_in_ext_destroy(h);
  else if (h->type == IO_IN_RECORDS_TYPE)
    io_in_records_destroy(h);
  else if (h->type == IO_IN_LIST_TYPE)
    io_in_destroy_from_list(h);
  else if (h->type == IO_IN_CB_TYPE)
    io_in_destroy_from_cb(h);
  else {
    if (h->base)
      io_in_base_destroy(h->base);
    if (h->lz4)
      lz4_destroy(h->lz4);
    if (h->out && h->destroy_out)
      h->destroy_out(h->out);
    if (h->group_bh)
      aml_buffer_destroy(h->group_bh);
    if (h->bh)
      aml_buffer_destroy(h->bh);

    aml_free(h);
  }
}

void reset_block(io_in_buffer_t *b) {
  memmove(b->buffer, b->buffer + b->pos, b->used - b->pos);
  b->used -= b->pos;
  b->pos = 0;
}

static int read_lz4_block(io_in_t *h, io_in_buffer_t *dest) {
  uint32_t *s = (uint32_t *)io_in_base_read(h->base, 4);
  if (!s)
    return 0;

  uint32_t length = *s;
  bool compressed = true;
  if (length & 0x80000000U) {
    compressed = false;
    length -= 0x80000000U;
  }
  if (!length)
    return 0;

  length += h->block_header_size;
  char *p = io_in_base_read(h->base, length);
  if (!p)
    return 0;

  char *dp = dest->buffer + dest->used;
  int n = lz4_decompress(h->lz4, p, length, dp, h->block_size, compressed);
  if (n < 0)
    return -1;

  dest->used += n;
  return n;
}

static void fill_blocks(io_in_t *h, io_in_buffer_t *dest) {
  while (1) {
    if (dest->used + h->block_size <= dest->size) {
      if (read_lz4_block(h, dest) <= 0) {
        dest->eof = true;
        return;
      }
    } else
      return;
  }
}

void _io_in_empty(io_in_t *h) {
  // if (h->group_bh)
  //   aml_buffer_destroy(h->group_bh);
  h->current = NULL;
  h->current_tmp = NULL;
  h->num_current = 0;
  h->num_current_tmp = 0;
  h->advance = empty_record;
  h->advance_unique = empty_record_unique;
  h->advance_unique_tmp = h->advance_unique;
  h->advance_tmp = h->advance;
}

io_in_t *io_in_empty(void) {
  io_in_t *h = (io_in_t *)aml_zalloc(sizeof(io_in_t));
  _io_in_empty(h);
  return h;
}

io_record_t *advance_reduced(io_in_t *h);

io_in_t *io_in_quick_init(const char *filename, io_format_t format, size_t buffer_size) {
  io_in_options_t opts;
  io_in_options_init(&opts);
  io_in_options_format(&opts, format);
  io_in_options_buffer_size(&opts, buffer_size);
  return io_in_init(filename, &opts);
}

io_in_t *_io_in_init(const char *filename, int fd, bool can_close, void *buf,
                     size_t buf_len, bool can_free, io_in_options_t *options) {
  io_in_options_t opts;
  if (!options) {
    options = &opts;
    io_in_options_init(options);
  }

  if (!filename && fd == -1 && !buf)
    abort();

  bool is_lz4 = false;
  if ((options->lz4 && (buf || !filename)) ||
      io_extension(filename, "lz4")) {
    if (!options->compressed_buffer_size) {
      options->compressed_buffer_size = options->buffer_size;
    }
    is_lz4 = true;
    size_t tmp = options->buffer_size;
    options->buffer_size = options->compressed_buffer_size;
    options->compressed_buffer_size = tmp;
  }

  io_in_base_t *base = NULL;
  if (buf) {
    if (options->gz)
      abort();

    base = io_in_base_init_from_buffer((char *)buf, buf_len, can_free);
  } else {
    if ((!filename && options->gz) || io_extension(filename, "gz"))
      base = io_in_base_init_gz(filename, fd, can_close, options->buffer_size);
    else
      base = io_in_base_init(filename, fd, can_close, options->buffer_size);
  }
  io_in_t *h = NULL;
  if (!base) {
    if (options->abort_on_file_not_found)
      abort();

    h = (io_in_t *)aml_zalloc(sizeof(io_in_t));
    _io_in_empty(h);
    return h;
  }

  if (is_lz4) {
    char *headerp = io_in_base_read(base, 7);
    if (!headerp) {
      io_in_base_destroy(base);
      if (options->abort_on_file_empty)
        abort();
      h = (io_in_t *)aml_zalloc(sizeof(io_in_t));
      _io_in_empty(h);
      return h;
    }
    lz4_t *lz4 = lz4_init_decompress(headerp, 7);
    if (!lz4) {
      io_in_base_destroy(base);
      if (options->abort_on_error)
        abort();
      h = (io_in_t *)aml_zalloc(sizeof(io_in_t));
      _io_in_empty(h);
      return h;
    }
    uint32_t buffer_size = options->buffer_size;
    uint32_t block_size = lz4_block_size(lz4);
    uint32_t block_header_size = lz4_block_header_size(lz4);
    uint32_t compressed_size = lz4_compressed_size(lz4);
    if (buffer_size < compressed_size + block_header_size + 4) {
      // printf("reinit\n");
      base = io_in_base_reinit(base, compressed_size + block_header_size + 4);
    }
    buffer_size = options->compressed_buffer_size;
    if (buffer_size < (block_size * 2) + 100)
      buffer_size = (block_size * 2) + 100;

    h = (io_in_t *)aml_malloc(sizeof(io_in_t) + buffer_size + 1);
    memset(h, 0, sizeof(*h));
    h->lz4 = lz4;
    h->buf.buffer = (char *)(h + 1);
    h->buf.size = buffer_size;
    h->block_size = block_size;
    h->block_header_size = block_header_size;

    h->options = *options;
    h->base = base;
    h->rec.tag = options->tag;
    if (options->format < 0) {
      h->delimiter = (-options->format) - 1;
      h->advance = _advance_delimited_lz4;
    } else if (options->format > 0) {
      h->fixed = options->format;
      h->advance = _advance_fixed_lz4;
    } else
      h->advance = _advance_prefix_lz4;
    // printf("%p filling\n", h);
    fill_blocks(h, &(h->buf));
    // printf("%p filled: %lu, %s\n", h, buffer_size, filename ? filename : "");
  } else {
    h = (io_in_t *)aml_zalloc(sizeof(io_in_t));
    h->options = *options;
    h->base = base;
    h->rec.tag = options->tag;
    if (options->format < 0) {
      h->delimiter = (-options->format) - 1;
      h->advance = _advance_delimited;
    } else if (options->format > 0) {
      h->fixed = options->format;
      h->advance = _advance_fixed;
    } else
      h->advance = _advance_prefix;
  }
  h->advance_unique = io_in_advance_unique_single;
  h->advance_unique_tmp = h->advance_unique;

  if (options->reducer) {
    h->reducer_bh = aml_buffer_init(256);
    h->reducer_group_bh = aml_buffer_init(256);
    h->sub_advance = h->advance;
    h->advance = advance_reduced;
  }

  h->advance_tmp = h->advance;
  return h;
}

io_record_t *advance_file_list(io_in_t *hp) {
  io_in_list_t *h = (io_in_list_t *)hp;
  if (!h)
    return NULL;

  h->num_current = 0;
  io_record_t *r = h->cur_in->advance(h->cur_in);
  if (!r) {
    io_in_destroy(h->cur_in);
    h->cur_in = NULL;
    io_in_options_t opts;
    while (!h->cur_in && h->filep < h->fileep) {
      opts = h->options;
      if (h->filep[0].size < opts.buffer_size)
        opts.buffer_size = h->filep[0].size;
      opts.tag = h->filep[0].tag;
      h->cur_in = io_in_init(h->filep[0].filename, &opts);
      h->filep++;
    }
    if (!h->cur_in) {
      _io_in_empty(hp);
      return NULL;
    }
    r = h->cur_in->advance(h->cur_in);
  }
  h->current = r;
  h->num_current = 1;
  return r;
}

io_record_t *advance_unique_file_list(io_in_t *hp, size_t *num_r) {
  io_in_list_t *h = (io_in_list_t *)hp;
  if (!h) {
    *num_r = 0;
    return NULL;
  }
  io_record_t *r = h->advance(hp);
  *num_r = h->num_current;
  return r;
}

io_in_t *io_in_init_from_list(io_file_info_t *files, size_t num_files,
                              io_in_options_t *options) {
  if (!num_files)
    return NULL;

  io_in_options_t opts2;
  if (!options) {
    options = &opts2;
    io_in_options_init(options);
  }

  io_in_list_t *h = (io_in_list_t *)aml_zalloc(
      sizeof(io_in_list_t) + (sizeof(io_file_info_t) * num_files));
  h->file_list = (io_file_info_t *)(h + 1);
  h->type = IO_IN_LIST_TYPE;
  h->options = *options;
  io_file_info_t *fp = h->file_list;
  for (size_t i = 0; i < num_files; i++) {
    if (files[i].size) {
      *fp = files[i];
      fp->filename = aml_strdup(fp->filename);
      fp++;
    }
  }
  h->filep = h->file_list;
  h->fileep = fp;
  io_in_options_t opts;
  while (!h->cur_in && h->filep < h->fileep) {
    opts = h->options;
    if (h->filep[0].size < opts.buffer_size)
      opts.buffer_size = h->filep[0].size;
    opts.tag = h->filep[0].tag;
    h->cur_in = io_in_init(h->filep[0].filename, &opts);
    h->filep++;
  }
  h->advance = advance_file_list;
  h->advance_unique = advance_unique_file_list;
  h->advance_unique_tmp = h->advance_unique;
  h->advance_tmp = h->advance;
  return (io_in_t *)h;
}

void io_in_destroy_from_list(io_in_t *hp) {
  io_in_list_t *h = (io_in_list_t *)hp;
  if (!h)
    return;
  if (h->cur_in)
    io_in_destroy(h->cur_in);
  h->filep = h->file_list;
  while (h->filep < h->fileep) {
    aml_free(h->filep->filename);
    h->filep++;
  }
  aml_free(h);
}

io_record_t *advance_cb(io_in_t *hp) {
  io_in_cb_t *h = (io_in_cb_t *)hp;
  if (!h)
    return NULL;

  h->num_current = 0;
  io_record_t *r = h->cur_in->advance(h->cur_in);
  if (!r) {
    io_in_destroy(h->cur_in);
    h->cur_in = h->cb(h->arg);
    if (!h->cur_in) {
      _io_in_empty(hp);
      return NULL;
    }
    r = h->cur_in->advance(h->cur_in);
  }
  h->current = r;
  h->num_current = 1;
  return r;
}

io_record_t *advance_unique_cb(io_in_t *hp, size_t *num_r) {
  if (!hp) {
    *num_r = 0;
    return NULL;
  }
  io_in_cb_t *h = (io_in_cb_t *)hp;
  io_record_t *r = h->advance(hp);
  *num_r = h->num_current;
  return r;
}

io_in_t *io_in_init_from_cb(io_in_init_cb cb, void *arg) {
  io_in_t *cur = cb(arg);
  if(!cur)
    return NULL;

  io_in_cb_t *h = (io_in_cb_t *)aml_zalloc(
      sizeof(io_in_cb_t));
  h->cb = cb;
  h->arg = arg;
  h->type = IO_IN_CB_TYPE;
  h->cur_in = cur;

  h->advance = advance_cb;
  h->advance_unique = advance_unique_cb;
  h->advance_unique_tmp = h->advance_unique;
  h->advance_tmp = h->advance;
  return (io_in_t *)h;
}

void io_in_destroy_from_cb(io_in_t *hp) {
  io_in_cb_t *h = (io_in_cb_t *)hp;
  if (!h)
    return;
  if (h->cur_in)
    io_in_destroy(h->cur_in);
  aml_free(h);
}


static io_record_t *count_and_advance(io_in_t *h) {
  h->record_num++;
  if (h->record_num <= h->limit)
    return h->count_advance(h);

  _io_in_empty(h);
  return NULL;
}

void io_in_limit(io_in_t *h, size_t limit) {
  h->limit = limit;
  h->count_advance = h->advance;
  h->advance = h->advance_tmp = count_and_advance;
}

io_record_t *advance_reduced(io_in_t *h) {
  aml_buffer_t *bh = h->reducer_group_bh;
  io_compare_cb compare = h->options.compare;
  void *arg = h->options.compare_arg;
  aml_buffer_clear(bh);
  io_record_t r1;
  io_record_t *r;
  io_record_t *res;
  size_t num_records;
  do {
    if ((r = h->sub_advance(h)) == NULL) {
      _io_in_empty(h);
      return NULL;
    }

    aml_buffer_set(bh, &(r->length), sizeof(r->length));
    aml_buffer_append(bh, &(r->tag), sizeof(r->tag));
    aml_buffer_append(bh, r->record, r->length);
    aml_buffer_appendc(bh, 0);
    num_records = 1;
    r1 = *r;
    while ((r = h->sub_advance(h)) != NULL) {
      char *b = aml_buffer_data(bh);
      b += sizeof(r->length) + sizeof(r->tag);
      r1.record = b;

      if (compare(&r1, r, arg)) {
        io_in_reset(h);
        break;
      }
      aml_buffer_append(bh, &(r->length), sizeof(r->length));
      aml_buffer_append(bh, &(r->tag), sizeof(r->tag));
      aml_buffer_append(bh, r->record, r->length);
      aml_buffer_appendc(bh, 0);
      num_records++;
    }
    size_t len = aml_buffer_length(bh);
    res = (io_record_t *)aml_buffer_append_alloc(
        bh, num_records * sizeof(io_record_t));
    io_record_t *rp = res;
    char *ptr = aml_buffer_data(bh);
    char *endp = ptr + len;
    while (ptr < endp) {
      rp->length = (*(uint32_t *)(ptr));
      ptr += sizeof(uint32_t);
      rp->tag = (*(int32_t *)(ptr));
      ptr += sizeof(int32_t);
      rp->record = ptr;
      ptr += rp->length;
      ptr++;
      rp++;
    }
  } while (!h->options.reducer(&(h->rec), res, num_records, h->reducer_bh,
                               h->options.reducer_arg));

  h->num_current = 1;
  h->current = &(h->rec);
  return h->current;
}

io_in_t *io_in_init(const char *filename, io_in_options_t *options) {
  return _io_in_init(filename, -1, true, NULL, 0, false, options);
}

io_in_t *io_in_init_with_fd(int fd, bool can_close, io_in_options_t *options) {
  return _io_in_init(NULL, fd, can_close, NULL, 0, false, options);
}

io_in_t *io_in_init_with_buffer(void *buf, size_t len, bool can_free,
                                io_in_options_t *options) {
  return _io_in_init(NULL, -1, false, buf, len, can_free, options);
}

static inline char *end_of_block(io_in_t *h, int32_t *rlen, char *p, char *ep,
                                 bool required) {
  if (required)
    return NULL;
  else {
    h->zerop = ep;
    h->zero = *ep;
    *ep = 0;
    *rlen = (ep - p);
    return p;
  }
}

static inline void cleanup_last_read(io_in_t *h) {
  if (h->bh) {
    aml_buffer_destroy(h->bh);
    h->bh = NULL;
  }

  if (h->zerop) {
    (*h->zerop) = h->zero;
    h->zerop = NULL;
  }
}

char *io_in_lz4_read_delimited(io_in_t *h, int32_t *rlen, int delim,
                               bool required) {
  bool csv = false;
  if(delim >= 256) {
    csv = true;
    delim -= 256;
  }

  cleanup_last_read(h);
  *rlen = 0;

  io_in_buffer_t *b = &(h->buf);
  char *p = b->buffer + b->pos;

  char *sp = p;
  // 2. search for delimiter between pos/used
  char *ep = b->buffer + b->used;
  while (p < ep) {
    if (*p == '\"') {
       if(csv) {
          p++;
        encoded_quote1:
          while(p < ep && *p != '\"')
            p++;
          if(p+1 < ep && p[1] == '\"') {
            p += 2;
            goto encoded_quote1;
          }
      }
      if(p < ep)
        p++;
    }
    else if (*p != delim)
      p++;
    else {
      *rlen = (p - sp);
      b->pos += (*rlen) + 1;
      h->zerop = p;
      h->zero = *p;
      *p = 0;
      return sp;
    }
  }

  // 3. if finished, there is no more data to read, return what is present
  if (b->eof) {
    b->pos = b->used;
    return end_of_block(h, rlen, sp, p, required);
  }

  // 4. reset the block such that the pos starts at zero and fill rest of
  //    block.  If pos was zero, nothing to do here
  if (b->pos > 0) {
    reset_block(b);
    sp = b->buffer;
    p = sp + b->used;
    //b->pos = b->used;
    fill_blocks(h, b);
    char *ep = sp + b->used;
    while (p < ep) {
      if (*p == '\"') {
         if(csv) {
            p++;
          encoded_quote2:
            while(p < ep && *p != '\"')
              p++;
            if(p+1 < ep && p[1] == '\"') {
              p += 2;
              goto encoded_quote2;
            }
        }
        if(p < ep)
          p++;
      }
      else if (*p != delim)
        p++;
      else {
        *rlen = (p - sp);
        b->pos += (*rlen) + 1;
        h->zerop = p;
        h->zero = *p;
        *p = 0;
        return sp;
      }
    }
    if (b->eof) {
      b->pos = b->used;
      return end_of_block(h, rlen, sp, p, required);
    }
  }
  // 5. The delimiter was not found in b->size bytes, create a tmp buffer
  //    that can be used to handle full result.  Make 1.5x because it'll have
  //    to grow at least once most of the time if less than this.
  h->bh = aml_buffer_init((b->used * 3) / 2);
  while (1) {
    aml_buffer_append(h->bh, b->buffer, b->used);
    b->used = 0;
    b->pos = 0;
    fill_blocks(h, b);
    p = b->buffer;
    sp = p;
    ep = p + b->used;
    while (p < ep) {
      if (*p == '\"') {
         if(csv) {
            p++;
          encoded_quote3:
            while(p < ep && *p != '\"')
              p++;
            if(p+1 < ep && p[1] == '\"') {
              p += 2;
              goto encoded_quote3;
            }
        }
        if(p < ep)
          p++;
      }
      else if (*p != delim)
        p++;
      else {
        size_t length = (p - sp);
        b->pos += length + 1;
        aml_buffer_append(h->bh, b->buffer, length);
        *rlen = aml_buffer_length(h->bh);
        return aml_buffer_data(h->bh);
      }
    }
    if (b->eof) {
      b->pos = b->used;
      if (required) {
        aml_buffer_destroy(h->bh);
        return NULL;
      } else {
        aml_buffer_append(h->bh, sp, p - sp);
        *rlen = aml_buffer_length(h->bh);
        return aml_buffer_data(h->bh);
      }
    }
  }
  // should not happen
  return NULL;
}


char *io_in_use_buffer(io_in_t *h, io_in_buffer_t *b, uint32_t len, int32_t *rlen) {
  /* where length is greater than the
     internal buffer.  In this case, a buffer is used and
     all data is copied into it. */
  h->bh = aml_buffer_init(len);
  aml_buffer_resize(h->bh, len);
  io_in_buffer_t tmp;
  tmp.buffer = aml_buffer_data(h->bh);
  tmp.size = len;
  tmp.used = b->used - b->pos;
  tmp.pos = tmp.used;
  if (tmp.used)
    memcpy(tmp.buffer, b->buffer + b->pos, tmp.used);
  b->pos = b->used = 0;
  len -= tmp.used;
  while(len) {
    fill_blocks(h, b);
    if (b->used == 0 && b->eof) {
        if (rlen)
            *rlen = tmp.pos;
        return tmp.buffer;
    }
    else if (len <= b->used) {
        memcpy(tmp.buffer + tmp.used, b->buffer, len);
        b->pos = len;
        tmp.pos += len;
        tmp.used += len;
        break;
    } else {
        memcpy(tmp.buffer + tmp.used, b->buffer, b->used);
        tmp.used += b->used;
        tmp.pos += b->used;
        len -= b->used;
        b->pos = b->used = 0;
    }
  }
  if(rlen)
    *rlen = tmp.pos;
  return tmp.buffer;
}


char *io_in_lz4_readz(io_in_t *h, int32_t *rlen, uint32_t len) {
  cleanup_last_read(h);

  io_in_buffer_t *b = &(h->buf);
  *rlen = 0;

  char *p = b->buffer + b->pos;
  if (b->pos + len <= b->used) {
    b->pos += len;
    *rlen = len;
    char *ep = p + len;
    h->zero = *ep;
    h->zerop = ep;
    *ep = 0;
    return p;
  } else if (len > b->size) {
    if (b->eof) {
      *rlen = b->used - b->pos;
      b->pos = b->used;
      char *ep = p + (*rlen);
      h->zero = *ep;
      h->zerop = ep;
      *ep = 0;
      return p;
    }

    return io_in_use_buffer(h, b, len, rlen);
  } else {
    if (b->eof) {
      *rlen = b->used - b->pos;
      b->pos = b->used;
      char *ep = p + (*rlen);
      h->zero = *ep;
      h->zerop = ep;
      *ep = 0;
      return p;
    }
    reset_block(b);
    fill_blocks(h, b);
    if (len > b->used)
      return io_in_use_buffer(h, b, len, rlen);
    b->pos = len;
    *rlen = len;
    char *ep = b->buffer + len;
    h->zero = *ep;
    h->zerop = ep;
    *ep = 0;
    return b->buffer;
  }
}

char *io_in_lz4_read(io_in_t *h, uint32_t len) {
  cleanup_last_read(h);

  io_in_buffer_t *b = &(h->buf);

  char *p = b->buffer + b->pos;
  if (b->pos + len <= b->used) {
    b->pos += len;
    return p;
  } else if (len > b->size) {
    if (b->eof) {
      b->pos = b->used;
      return NULL;
    }
    return io_in_use_buffer(h, b, len, NULL);
  } else {
    if (b->eof) {
      b->pos = b->used;
      return NULL;
    }
    reset_block(b);
    fill_blocks(h, b);
    if (len > b->used)
      return io_in_use_buffer(h, b, len, NULL);
    b->pos = len;
    return b->buffer;
  }
}

void io_in_options_init(io_in_options_t *h) {
  memset(h, 0, sizeof(*h));
  h->buffer_size = 128 * 1024;
  h->compressed_buffer_size = 0;

  h->full_record_required = true;
  h->format = 0;
  h->abort_on_error = false;
  h->abort_on_partial_record = false;
  h->abort_on_file_not_found = false;
  h->abort_on_file_empty = false;
  h->tag = 0;
  h->gz = false;
  h->lz4 = false;
}

void io_in_options_buffer_size(io_in_options_t *h, size_t buffer_size) {
  h->buffer_size = buffer_size;
}

void io_in_options_format(io_in_options_t *h, io_format_t format) {
  h->format = format;
}

void io_in_options_abort_on_error(io_in_options_t *h) {
  h->abort_on_error = true;
}

void io_in_options_allow_partial_records(io_in_options_t *h) {
  h->full_record_required = false;
  h->abort_on_partial_record = false;
}

void io_in_options_abort_on_partial_record(io_in_options_t *h) {
  h->abort_on_partial_record = true;
}

void io_in_options_abort_on_file_not_found(io_in_options_t *h) {
  h->abort_on_file_not_found = true;
}

void io_in_options_abort_on_file_empty(io_in_options_t *h) {
  h->abort_on_file_empty = true;
}

void io_in_options_tag(io_in_options_t *h, int tag) { h->tag = tag; }

void io_in_options_gz(io_in_options_t *h, size_t buffer_size) { h->gz = true; }

void io_in_options_lz4(io_in_options_t *h, size_t buffer_size) {
  h->lz4 = true;
  h->compressed_buffer_size = buffer_size;
}

void io_in_options_compressed_buffer_size(io_in_options_t *h,
                                          size_t buffer_size) {
  h->compressed_buffer_size = buffer_size;
}

void io_in_options_reducer(io_in_options_t *h, io_compare_cb compare,
                           void *compare_arg, io_reducer_cb reducer,
                           void *reducer_arg) {
  h->compare = compare;
  h->compare_arg = compare_arg;
  h->reducer = reducer;
  h->reducer_arg = reducer_arg;
}

/*****************************************************************************
  io_in_ext... functionality

  in_heap_t tracks a heap full of io_in_t objects and sorts based upon given
  comparison function.
*/

struct in_heap_s;
typedef struct in_heap_s in_heap_t;

struct in_heap_s {
  ssize_t size;
  ssize_t max_size;
  io_in_t **heap;
  io_compare_cb compare;
  void *compare_arg;
};

static inline void in_heap_init(in_heap_t *h, ssize_t mx,
                                io_compare_cb compare, void *arg) {
  h->size = 0;
  if (mx < 2)
    mx = 2;
  h->max_size = mx;
  h->heap = (io_in_t **)aml_malloc((mx + 1) * sizeof(io_in_t *));
  h->compare = compare;
  h->compare_arg = arg;
}

static inline void in_heap_clear(in_heap_t *h) { h->size = 0; }

static inline size_t in_heap_max(in_heap_t *h) { return h->max_size; }

static inline void in_heap_destroy(in_heap_t *h) {
  if (!h)
    return;

  aml_free(h->heap);
}

static inline io_in_t **in_heap_base(in_heap_t *h) { return h->heap; }

static inline size_t in_heap_size(in_heap_t *h) { return h->size; }

static inline int in_heap_compare(in_heap_t *h, io_in_t *a, io_in_t *b) {
  return h->compare(a->current, b->current, h->compare_arg);
}

static inline int in_heap_compare2(in_heap_t *h, io_record_t *a,
                                   io_in_t *b) {
  return h->compare(a, b->current, h->compare_arg);
}

void test_heap(in_heap_t *h) {
  for (size_t i = 1; i <= h->size; i++) {
    if (h->heap[i] == NULL)
      abort();
  }
}

static inline void in_heap_push(in_heap_t *h, io_in_t *item) {
  if (h->size >= h->max_size) {
    h->max_size = h->size * 2;
    io_in_t **heap =
        (io_in_t **)aml_malloc((h->max_size + 1) * sizeof(io_in_t *));
    memcpy(heap, h->heap, ((h->size + 1) * sizeof(io_in_t *)));
    aml_free(h->heap);
    h->heap = heap;
  }
  h->size++;
  ssize_t num = h->size;
  io_in_t **heap = h->heap;
  // io_in_t *tmp = heap[num];
  heap[num] = item;
  // heap[0] = tmp;
  ssize_t i = num;
  ssize_t j = i >> 1;
  io_in_t *tmp;

  while (j > 0 && in_heap_compare(h, heap[i], heap[j]) < 0) {
    tmp = heap[i];
    heap[i] = heap[j];
    heap[j] = tmp;
    i = j;
    j = j >> 1;
  }
}

static inline io_in_t *in_heap_pop(in_heap_t *h) {
  h->size--;
  ssize_t num = h->size;
  io_in_t **heap = h->heap;
  io_in_t *r = heap[1];
  heap[1] = heap[num + 1];

  ssize_t i = 1;
  ssize_t j = i << 1;
  ssize_t k = j + 1;

  if (k <= num && in_heap_compare(h, heap[k], heap[j]) < 0)
    j = k;

  while (j <= num && in_heap_compare(h, heap[j], heap[i]) < 0) {
    io_in_t *tmp = heap[i];
    heap[i] = heap[j];
    heap[j] = tmp;

    i = j;
    j = i << 1;
    k = j + 1;
    if (k <= num && in_heap_compare(h, heap[k], heap[j]) < 0)
      j = k;
  }
  return r;
}

/*
  The io_in_ext_t structure needs to share the same members as io_in_s up
  through group_bh.
*/

typedef struct {
  io_in_options_t options;
  int type;
  io_record_t rec;
  io_record_t *current;
  size_t num_current;
  io_record_t *current_tmp;
  size_t num_current_tmp;
  io_in_advance_cb advance;
  io_in_advance_cb advance_tmp;
  io_in_advance_unique_cb advance_unique;
  io_in_advance_unique_cb advance_unique_tmp;
  io_in_advance_cb count_advance;
  size_t limit;
  size_t record_num;
  io_out_t *out;
  void (*destroy_out)(io_out_t *out);
  aml_buffer_t *group_bh;

  io_in_t **active;
  size_t num_active;
  size_t active_size;
  io_record_t *r;

  in_heap_t heap;

  aml_buffer_t *reducer_bh;
  io_reducer_cb reducer;
  void *reducer_arg;

  io_compare_cb compare;
  void *compare_arg;
} io_in_ext_t;

io_in_t *io_in_ext_init(io_compare_cb compare, void *arg,
                        io_in_options_t *options) {
  io_in_options_t opts;
  if (!options) {
    options = &opts;
    io_in_options_init(options);
  }

  io_in_ext_t *h = (io_in_ext_t *)aml_zalloc(sizeof(io_in_ext_t));
  h->type = IO_IN_EXT_TYPE;
  h->compare = compare;
  h->compare_arg = arg;
  h->options = *options;
  in_heap_init(&(h->heap), 0, compare, arg);

  _io_in_empty((io_in_t *)h);
  return (io_in_t *)h;
}

void io_in_ext_destroy(io_in_t *hp) {
  io_in_ext_t *h = (io_in_ext_t *)hp;
  if (!h)
    return;

  for (size_t i = 0; i < h->num_active; i++)
    io_in_destroy(h->active[i]);

  if (h->active)
    aml_free(h->active);

  in_heap_t *heap = &(h->heap);

  while (in_heap_size(heap))
    io_in_destroy(in_heap_pop(heap));

  in_heap_destroy(heap);

  if (h->reducer_bh)
    aml_buffer_destroy(h->reducer_bh);

  if (h->group_bh)
    aml_buffer_destroy(h->group_bh);

  if (h->out)
    io_out_destroy(h->out);

  aml_free(h);
}

static void move_active_to_heap(io_in_ext_t *h, bool advance) {
  if (!h)
    return;

  in_heap_t *heap = &(h->heap);
  for (size_t i = 0; i < h->num_active; i++) {
    io_in_t *in = h->active[i];
    if (advance) {
      if (!io_in_advance(in)) {
        io_in_destroy(in);
        continue;
      }
    }
    in_heap_push(heap, in);
  }
  h->num_active = 0;
}

io_record_t *io_in_ext_advance(io_in_t *hp) {
  if (!hp)
    return NULL;

  io_in_ext_t *h = (io_in_ext_t *)hp;
  move_active_to_heap(h, true);
  in_heap_t *heap = &(h->heap);
  if (in_heap_size(heap)) {
    io_in_t *in = in_heap_pop(heap);
    h->active[0] = in;
    h->num_active = 1;
    h->current = io_in_current(in);
    return h->current;
  }
  _io_in_empty(hp);
  return NULL;
}

io_record_t *io_in_ext_advance_unique(io_in_t *hp, size_t *num_r) {
  io_record_t *first = io_in_ext_advance(hp);
  if (!first)
    return NULL;

  io_in_ext_t *h = (io_in_ext_t *)hp;
  in_heap_t *heap = &(h->heap);
  io_in_t **activep = h->active + 1;
  io_record_t *rp = h->r;
  *rp++ = *first;
  io_in_t *in = NULL;
  while (in_heap_size(heap)) {
    in = in_heap_pop(heap);
    if (!in_heap_compare2(heap, first, in)) {
      *activep++ = in;
      *rp++ = *io_in_current(in);
    } else {
      in_heap_push(heap, in);
      break;
    }
  }
  h->num_active = activep - h->active;
  h->num_current = h->num_active;
  h->current = h->r;
  *num_r = h->num_current;
  return h->current;
}

io_record_t *io_in_ext_advance_reduce(io_in_t *hp) {
  io_in_ext_t *h = (io_in_ext_t *)hp;
  while (1) {
    size_t num_r = 0;
    io_record_t *r = io_in_ext_advance_unique(hp, &num_r);
    if (!r)
      return NULL;
    if (h->reducer(&h->rec, r, num_r, h->reducer_bh, h->reducer_arg)) {
      r = h->current = &(h->rec);
      h->num_current = 1;
      return r;
    }
  }
  return NULL; // should not happen
}

void io_in_ext_add(io_in_t *hp, io_in_t *in, int tag) {
  io_in_ext_t *h = (io_in_ext_t *)hp;
  if (!h || !in || h->type != IO_IN_EXT_TYPE)
    return;

  in->rec.tag = tag;
  in->options.tag = tag;

  io_in_reset(in);
  if (!io_in_advance(in)) {
    io_in_destroy(in);
    return;
  }

  in_heap_t *heap = &(h->heap);
  move_active_to_heap(h, false);
  in_heap_push(heap, in);

  if (h->active_size < in_heap_size(heap)) {
    if (h->active)
      aml_free(h->active);
    h->active_size = in_heap_max(heap);
    h->active = (io_in_t **)aml_malloc(
        (sizeof(io_in_t *) + sizeof(io_record_t)) * h->active_size);
    h->r = (io_record_t *)(h->active + h->active_size);
  }
  h->num_active = 0;

  if (h->advance == empty_record) {
    if (h->reducer)
      h->advance = h->advance_tmp = io_in_ext_advance_reduce;
    else
      h->advance = h->advance_tmp = io_in_ext_advance;
    h->advance_unique = h->advance_unique_tmp = io_in_ext_advance_unique;
  }
}

void io_in_ext_reducer(io_in_t *hp, io_reducer_cb reducer, void *arg) {
  io_in_ext_t *h = (io_in_ext_t *)hp;
  if (!h || h->type != IO_IN_EXT_TYPE)
    return;

  if (h->advance == io_in_ext_advance)
    h->advance = h->advance_tmp = io_in_ext_advance_reduce;

  h->reducer = reducer;
  h->reducer_arg = arg;
  h->reducer_bh = aml_buffer_init(1024);
}

void io_in_ext_keep_first(io_in_t *hp) {
  io_in_ext_t *h = (io_in_ext_t *)hp;
  if (!h || h->type != IO_IN_EXT_TYPE)
    return;

  if (h->advance == io_in_ext_advance)
    h->advance = h->advance_tmp = io_in_ext_advance_reduce;

  h->reducer = io_keep_first;
  h->reducer_arg = NULL;
  h->reducer_bh = NULL;
}

/* io_in_records_t - this allows a cursor to be created with an array of
   io_record_t structures */
typedef struct {
  io_in_options_t options;
  int type;
  io_record_t rec;
  io_record_t *current;
  size_t num_current;
  io_record_t *current_tmp;
  size_t num_current_tmp;
  io_in_advance_cb advance;
  io_in_advance_cb advance_tmp;
  io_in_advance_unique_cb advance_unique;
  io_in_advance_unique_cb advance_unique_tmp;
  io_in_advance_cb count_advance;
  size_t limit;
  size_t record_num;
  io_out_t *out;
  void (*destroy_out)(io_out_t *out);
  aml_buffer_t *group_bh;

  io_record_t *records;
  size_t num_records;
  io_record_t *rp;
  io_record_t *ep;

  aml_buffer_t *reducer_bh;
} io_in_records_t;

void io_in_records_destroy(io_in_t *hp) {
  io_in_records_t *h = (io_in_records_t *)hp;
  if (h->reducer_bh)
    aml_buffer_destroy(h->reducer_bh);
  if (h->group_bh)
    aml_buffer_destroy(h->group_bh);
  if (h->out)
    io_out_destroy(h->out);
  aml_free(h);
}

io_record_t *io_in_records_advance(io_in_t *hp) {
  io_in_records_t *h = (io_in_records_t *)hp;
  if (h->rp < h->ep) {
    h->current = h->rp;
    h->rp++;
    h->num_current = 1;
    return h->current;
  }
  _io_in_empty(hp);
  return NULL;
}

io_record_t *io_in_records_advance_and_reduce(io_in_t *hp) {
  io_in_records_t *h = (io_in_records_t *)hp;
  io_in_options_t *opts = &h->options;
  io_compare_cb compare = opts->compare;
  void *arg = opts->compare_arg;
  while (h->rp < h->ep) {
    io_record_t *cur = h->rp;
    io_record_t *rp = cur;
    rp++;
    while (rp < h->ep && !compare(cur, rp, arg))
      rp++;
    h->rp = rp;
    if (opts->reducer(&(h->rec), cur, rp - cur, h->reducer_bh,
                      opts->reducer_arg)) {
      h->current = &(h->rec);
      h->num_current = 1;
      return h->current;
    }
  }
  _io_in_empty(hp);
  return NULL;
}

io_in_t *io_in_records_init(io_record_t *records, size_t num_records,
                            io_in_options_t *options) {
  io_in_options_t opts;
  if (!options) {
    options = &opts;
    io_in_options_init(options);
  }

  io_in_records_t *h = (io_in_records_t *)aml_zalloc(sizeof(io_in_records_t));
  h->options = *options;
  h->type = IO_IN_RECORDS_TYPE;
  h->records = records;
  h->num_records = num_records;
  h->rp = records;
  h->ep = records + num_records;
  h->options = *options;

  if (options->reducer) {
    h->reducer_bh = aml_buffer_init(256);
    h->advance = h->advance_tmp = io_in_records_advance_and_reduce;
  } else
    h->advance = h->advance_tmp = io_in_records_advance;
  h->advance_unique = h->advance_unique_tmp = io_in_advance_unique_single;
  return (io_in_t *)h;
}

void io_in_destroy_out(io_in_t *in, io_out_t *out,
                       void (*destroy_out)(io_out_t *out)) {
  in->out = out;
  in->destroy_out = destroy_out ? destroy_out : io_out_destroy;
}

void default_transform(io_in_t *in, io_out_t *out, void *arg) {
  io_record_t *r;
  while ((r = io_in_advance(in)) != NULL)
    io_out_write_record(out, r->record, r->length);
}

static void get_tmp_name(char *dest, size_t dest_len, const char *fmt) {
  static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  static int id = 0;
  int _id;
  pthread_mutex_lock(&mutex);
  _id = id;
  id++;
  pthread_mutex_unlock(&mutex);
  snprintf(dest, dest_len, fmt, _id);
}

io_in_t *io_in_transform(io_in_t *in, io_format_t format, size_t buffer_size,
                         io_compare_cb compare, void *compare_arg,
                         io_reducer_cb reducer, void *reducer_arg,
                         io_in_transform_cb transform, void *arg) {
  io_out_options_t opts;
  io_out_options_init(&opts);
  io_out_options_buffer_size(&opts, buffer_size);
  io_out_options_format(&opts, format);

  io_out_ext_options_t ext_opts;
  io_out_ext_options_init(&ext_opts);
  io_out_ext_options_compare(&ext_opts, compare, compare_arg);
  if (reducer)
    io_out_ext_options_reducer(&ext_opts, reducer, reducer_arg);
  char *name = (char *)aml_malloc(100);
  get_tmp_name(name, 100, "transform_%d.lz4");
  io_out_t *out = io_out_ext_init(name, &opts, &ext_opts);
  aml_free(name);

  if (!transform)
    transform = default_transform;

  transform(in, out, arg);
  io_in_destroy(in);
  in = io_out_in(out);
  io_in_destroy_out(in, out, NULL);
  return in;
}

void io_in_out(io_in_t *in, io_out_t *out) {
  io_record_t *r;
  while ((r = io_in_advance(in)) != NULL)
    io_out_write_record(out, r->record, r->length);
}

void io_in_out2(io_in_t *in, io_out_t *out, io_out_t *out2) {
  io_record_t *r;
  while ((r = io_in_advance(in)) != NULL) {
    io_out_write_record(out, r->record, r->length);
    io_out_write_record(out2, r->record, r->length);
  }
}

void io_in_out_custom(io_in_t *in, io_out_t *out, io_in_out_cb cb, void *arg) {
  io_record_t *r;
  while ((r = io_in_advance(in)) != NULL)
    cb(out, r, arg);
}

void io_in_out_custom2(io_in_t *in, io_out_t *out, io_out_t *out2,
                       io_in_out2_cb cb, void *arg) {
  io_record_t *r;
  while ((r = io_in_advance(in)) != NULL)
    cb(out, out2, r, arg);
}

void io_in_out_group(io_in_t *in, io_out_t *out, io_compare_cb compare,
                     void *compare_arg, io_in_out_group_cb group, void *arg) {
  size_t num_r = 0;
  io_record_t *r;
  bool more_records = false;
  while ((r = io_in_advance_group(in, &num_r, &more_records, compare,
                                  compare_arg)) != NULL)
    group(out, r, num_r, more_records, arg);
}

void io_in_out_group2(io_in_t *in, io_out_t *out, io_out_t *out2,
                      io_compare_cb compare, void *compare_arg,
                      io_in_out_group2_cb group, void *arg) {
  size_t num_r = 0;
  io_record_t *r;
  bool more_records = false;
  while ((r = io_in_advance_group(in, &num_r, &more_records, compare,
                                  compare_arg)) != NULL)
    group(out, out2, r, num_r, more_records, arg);
}
