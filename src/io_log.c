// SPDX-FileCopyrightText: 2019–2026 Andy Curtis <contactandyc@gmail.com>
// SPDX-FileCopyrightText: 2024–2025 Knode.ai
// SPDX-License-Identifier: Apache-2.0
//
// Maintainer: Andy Curtis <contactandyc@gmail.com>

#include "the-io-library/io_log.h"
#include "the-io-library/io_in.h"
#include "a-memory-library/aml_buffer.h"
#include "the-lz4-library/lz4.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/* ---------------- path helpers ---------------- */

static void split_dir_base(const char *full, char *dirbuf, size_t dirsz,
                           char *basebuf, size_t basesz) {
  const char *slash = strrchr(full, '/');
  if (slash) {
    size_t dlen = (size_t)(slash - full);
    if (dlen >= dirsz) dlen = dirsz - 1;
    memcpy(dirbuf, full, dlen);
    dirbuf[dlen] = '\0';
    snprintf(basebuf, basesz, "%s", slash + 1);
  } else {
    snprintf(dirbuf, dirsz, ".");
    snprintf(basebuf, basesz, "%s", full);
  }
}

static void make_path(char *dst, size_t dstsz, const char *dir, const char *name) {
  if (dir[0] == '\0' || (dir[0] == '.' && dir[1] == '\0'))
    snprintf(dst, dstsz, "%s", name);
  else
    snprintf(dst, dstsz, "%s/%s", dir, name);
}

static void rotated_name(char *dst, size_t dstsz, const char *dir, const char *base) {
  time_t now = time(NULL);
  struct tm tmv;
  localtime_r(&now, &tmv);
  char stamp[32];
  strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &tmv);
  char fname[512];
  snprintf(fname, sizeof(fname), "%s-%s", base, stamp);
  make_path(dst, dstsz, dir, fname);
}

/* ---------------- retention (directory-aware) ---------------- */

static void retention_cleanup_dir(const char *dir, const char *base_name, size_t max_files) {
  if (max_files == 0) return;

  DIR *dp = opendir(dir);
  if (!dp) return;

  struct dirent *e;
  struct stat st;
  struct item { char path[1024]; time_t mtime; } items[2048];
  size_t n = 0;

  while ((e = readdir(dp)) != NULL) {
    if (strncmp(e->d_name, base_name, strlen(base_name)) != 0) continue;
    if (!strstr(e->d_name, ".gz") && !strstr(e->d_name, ".lz4")) continue;

    char full[1024];
    make_path(full, sizeof(full), dir, e->d_name);
    if (stat(full, &st) == 0 && n < (sizeof(items)/sizeof(items[0]))) {
      snprintf(items[n].path, sizeof(items[n].path), "%s", full);
      items[n].mtime = st.st_mtime;
      n++;
    }
  }
  closedir(dp);

  if (n <= max_files) return;

  for (size_t i = 0; i + 1 < n; ++i)
    for (size_t j = i + 1; j < n; ++j)
      if (items[i].mtime > items[j].mtime) {
        struct item tmp = items[i]; items[i] = items[j]; items[j] = tmp;
      }

  for (size_t i = 0; i < n - max_files; ++i)
    remove(items[i].path);
}

/* ---- other helpers -------------------------------------------------------- */

static void strip_compression_ext(const char *name, aml_buffer_t *out) {
  const char *ext = strrchr(name, '.');
  if (ext && (!strcmp(ext, ".gz") || !strcmp(ext, ".lz4")))
    aml_buffer_set(out, name, ext - name);
  else
    aml_buffer_set(out, name, strlen(name));
  aml_buffer_appendc(out, 0);
}

static int compare_by_normalized_key(const void *a, const void *b) {
  const io_file_info_t *fa = (const io_file_info_t *)a;
  const io_file_info_t *fb = (const io_file_info_t *)b;

  aml_buffer_t *ka = aml_buffer_init(strlen(fa->filename) + 1);
  aml_buffer_t *kb = aml_buffer_init(strlen(fb->filename) + 1);
  strip_compression_ext(fa->filename, ka);
  strip_compression_ext(fb->filename, kb);

  int cmp = strcmp(aml_buffer_data(ka), aml_buffer_data(kb));
  if (cmp == 0)
    cmp = strcmp(fa->filename, fb->filename);

  aml_buffer_destroy(ka);
  aml_buffer_destroy(kb);
  return cmp;
}

static int preference_score(const char *fname) {
  if (!strstr(fname, ".gz") && !strstr(fname, ".lz4"))
    return 0;
  if (strstr(fname, ".lz4"))
    return 1;
  return 2;
}

static size_t dedup_in_place(io_file_info_t *files, size_t count) {
  size_t write_idx = 0;
  size_t i = 0;

  while (i < count) {
    aml_buffer_t *key_b = aml_buffer_init(strlen(files[i].filename) + 1);
    strip_compression_ext(files[i].filename, key_b);
    const char *key = aml_buffer_data(key_b);

    io_file_info_t *best = &files[i];
    int best_score = preference_score(best->filename);

    size_t j = i + 1;
    for (; j < count; j++) {
      aml_buffer_t *tmpkey = aml_buffer_init(strlen(files[j].filename) + 1);
      strip_compression_ext(files[j].filename, tmpkey);
      bool same = (strcmp(key, aml_buffer_data(tmpkey)) == 0);
      aml_buffer_destroy(tmpkey);
      if (!same)
        break;

      int score = preference_score(files[j].filename);
      if (score < best_score) {
        best = &files[j];
        best_score = score;
      }
    }

    if (write_idx != i)
      files[write_idx] = *best;
    write_idx++;

    aml_buffer_destroy(key_b);
    i = j;
  }
  return write_idx;
}

/* ---- main API ------------------------------------------------------- */

io_in_t *io_log_in(const char *base, bool include_active) {
  aml_buffer_t *dir_b = aml_buffer_init(256);
  aml_buffer_t *base_b = aml_buffer_init(128);
  const char *slash = strrchr(base, '/');
  if (slash) {
    aml_buffer_set(dir_b, base, slash - base);
    aml_buffer_set(base_b, slash + 1, strlen(slash + 1));
  } else {
    aml_buffer_set(dir_b, ".", 1);
    aml_buffer_set(base_b, base, strlen(base));
  }

  const char *dir = aml_buffer_data(dir_b);
  const char *base_name = aml_buffer_data(base_b);

  DIR *dp = opendir(dir);
  if (!dp) {
    perror("opendir");
    aml_buffer_destroy(dir_b);
    aml_buffer_destroy(base_b);
    return io_in_empty();
  }

  aml_buffer_t *files_b = aml_buffer_init(4096);
  size_t count = 0;
  struct dirent *e;

  while ((e = readdir(dp)) != NULL) {
    if (strncmp(e->d_name, base_name, strlen(base_name)) != 0)
      continue;
    if (!strstr(e->d_name, ".active") && !strchr(e->d_name, '-'))
      continue;

    aml_buffer_t *path = aml_buffer_init(strlen(dir) + strlen(e->d_name) + 3);
    aml_buffer_set(path, dir, strlen(dir));
    aml_buffer_appendc(path, '/');
    aml_buffer_append(path, e->d_name, strlen(e->d_name));
    aml_buffer_appendc(path, 0);

    io_file_info_t info;
    memset(&info, 0, sizeof(info));
    info.filename = aml_strdup(aml_buffer_data(path));
    struct stat st;
    if (stat(info.filename, &st) == 0)
      info.size = st.st_size;
    info.tag = 0;

    aml_buffer_append(files_b, &info, sizeof(info));
    count++;
    aml_buffer_destroy(path);
  }
  closedir(dp);

  if (count == 0) {
    aml_buffer_destroy(files_b);
    aml_buffer_destroy(dir_b);
    aml_buffer_destroy(base_b);
    return io_in_empty();
  }

  io_file_info_t *files = (io_file_info_t *)aml_buffer_data(files_b);
  qsort(files, count, sizeof(io_file_info_t), compare_by_normalized_key);
  size_t num_final = dedup_in_place(files, count);

  if (include_active) {
    aml_buffer_t *activepath =
        aml_buffer_init(strlen(dir) + strlen(base_name) + 16);
    aml_buffer_set(activepath, dir, strlen(dir));
    aml_buffer_appendc(activepath, '/');
    aml_buffer_append(activepath, base_name, strlen(base_name));
    aml_buffer_append(activepath, ".active", strlen(".active"));
    aml_buffer_appendc(activepath, 0);

    struct stat st;
    if (stat(aml_buffer_data(activepath), &st) == 0 && st.st_size > 0) {
      io_file_info_t info;
      memset(&info, 0, sizeof(info));
      info.filename = aml_strdup(aml_buffer_data(activepath));
      info.size = st.st_size;
      info.tag = 0;
      files[num_final++] = info;
    }
    aml_buffer_destroy(activepath);
  }

  io_in_options_t opts;
  io_in_options_init(&opts);
  io_in_options_format(&opts, io_prefix());
  io_in_t *in = io_in_init_from_list(files, num_final, &opts);

  for (size_t i = 0; i < count; i++)
    aml_free(files[i].filename);

  aml_buffer_destroy(files_b);
  aml_buffer_destroy(dir_b);
  aml_buffer_destroy(base_b);
  return in;
}

struct io_log_s {
  char *base_path;
  char dir_path[512];
  char base_name[256];
  char active_path[768];
  io_out_t *active;

  io_out_options_t opts;
  size_t rotate_size;
  time_t rotate_interval;
  size_t max_files;

  size_t bytes_written;
  time_t last_rotate;

  bool use_lz4;
  bool use_gz;
};

/* ---------------- crash recovery (.active) ---------------- */

static off_t scan_prefix_last_valid(const char *path) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) return 0;

  off_t last_good = 0;
  while (1) {
    uint32_t len;
    ssize_t r = read(fd, &len, sizeof(len));
    if (r == 0) break;
    if (r != (ssize_t)sizeof(len)) break;
    if (len < 12) break;

    char *buf = malloc(len);
    if (!buf) break;
    ssize_t pr = read(fd, buf, len);
    if (pr != (ssize_t)len) { free(buf); break; }

    uint32_t stored;
    memcpy(&stored, buf + 8, 4);
    bool has_ttl = stored & 1u;
    const void *remain = buf + 8 + 4;
    size_t remain_len = len - (8 + 4);
    uint32_t calc = lz4_hash32(remain, remain_len);
    if ((calc & ~1u) != (stored & ~1u)) {
      free(buf);
      break;
    }

    last_good += sizeof(len) + len;
    free(buf);
  }
  close(fd);
  return last_good;
}

static void recover_active_auto(const char *path) {
  struct stat st;
  if (stat(path, &st) != 0 || st.st_size == 0) return;
  off_t good = scan_prefix_last_valid(path);
  if (good < st.st_size) {
    int fd = open(path, O_WRONLY);
    if (fd >= 0) { ftruncate(fd, good); close(fd); }
  }
}

/* ---------------- async compression ---------------- */

typedef struct {
  char src_uncompressed[768];
  char dst_compressed[768];
  bool use_lz4;
  bool use_gz;
  char dir_path[512];
  char base_name[256];
  size_t max_files;
} compress_ctx_t;

static void *compress_thread_main(void *arg) {
  compress_ctx_t *ctx = (compress_ctx_t *)arg;

  io_in_options_t in_o;
  io_in_options_init(&in_o);
  // format of rotated files: whatever was written (prefix/delim/fixed)
  // we can leave format autodetection to filename suffixes if needed,
  // but since rotated is uncompressed copy of active, reading raw bytes
  // and re-emitting records would require knowing format. We simply
  // use io_prefix() path when io_out wrote prefix, otherwise convert raw
  // file by reading with io_in_quick_init would need delimiter/fixed.
  // Easiest: stream bytes via io_in in the same format the writer used.
  // Here we assume prefix (most common). For delimiter/fixed rotations,
  // compression is still correct if we copy raw bytes; we can do that
  // by reading with plain read/write. To keep things simple and generic,
  // we'll do a raw copy encapsulated as records of bytes.
  // Simpler: use file descriptor read/write loop.

  int in_fd = open(ctx->src_uncompressed, O_RDONLY);
  if (in_fd < 0) { free(ctx); return NULL; }

  io_out_options_t o;
  io_out_options_init(&o);
  if (ctx->use_lz4) io_out_options_lz4(&o, 1, s64kb, false, false);
  if (!ctx->use_lz4 && ctx->use_gz) io_out_options_gz(&o, 1);

  io_out_t *out = io_out_init(ctx->dst_compressed, &o);
  if (!out) { close(in_fd); free(ctx); return NULL; }

  char buf[256 * 1024];
  ssize_t n;
  while ((n = read(in_fd, buf, sizeof(buf))) > 0) {
    // write raw bytes through io_out_write (no record boundaries),
    // because rotated logs are opaque; compression is at byte-stream level.
    if (!io_out_write(out, buf, (size_t)n)) break;
  }
  // signal flush
  io_out_write(out, NULL, 0);

  io_out_destroy(out);
  close(in_fd);

  // remove uncompressed source after successful compression attempt
  remove(ctx->src_uncompressed);

  // retention after we wrote a new compressed file
  retention_cleanup_dir(ctx->dir_path, ctx->base_name, ctx->max_files);

  free(ctx);
  return NULL;
}

static void spawn_compress(const char *dir, const char *base_name,
                           const char *uncompressed_path,
                           bool use_lz4, bool use_gz,
                           size_t max_files) {
  compress_ctx_t *ctx = (compress_ctx_t *)malloc(sizeof(*ctx));
  memset(ctx, 0, sizeof(*ctx));
  snprintf(ctx->src_uncompressed, sizeof(ctx->src_uncompressed), "%s", uncompressed_path);
  snprintf(ctx->dir_path, sizeof(ctx->dir_path), "%s", dir);
  snprintf(ctx->base_name, sizeof(ctx->base_name), "%s", base_name);
  ctx->use_lz4 = use_lz4; ctx->use_gz = use_gz;
  ctx->max_files = max_files;

  // Build destination path with suffix
  char dst[768];
  snprintf(dst, sizeof(dst), "%s", uncompressed_path);
  strncat(dst, use_lz4 ? ".lz4" : ".gz", sizeof(dst) - strlen(dst) - 1);
  snprintf(ctx->dst_compressed, sizeof(ctx->dst_compressed), "%s", dst);

  pthread_t th;
  pthread_create(&th, NULL, compress_thread_main, ctx);
  pthread_detach(th);
}

/* Recompress any uncompressed rotated artifacts left from a crash. */
static void recompress_leftovers(const char *dir, const char *base_name,
                                 bool use_lz4, bool use_gz, size_t max_files) {
  DIR *dp = opendir(dir[0] ? dir : ".");
  if (!dp) return;
  struct dirent *e;
  while ((e = readdir(dp)) != NULL) {
    // match "<base>-<timestamp>" with no .gz/.lz4 and not ".active"
    if (strncmp(e->d_name, base_name, strlen(base_name)) != 0) continue;
    const char *rest = e->d_name + strlen(base_name);
    if (*rest != '-') continue;
    if (strstr(e->d_name, ".gz") || strstr(e->d_name, ".lz4")) continue;
    if (strstr(e->d_name, ".active")) continue;

    char full[768];
    make_path(full, sizeof(full), dir, e->d_name);
    // spawn a compressor per leftover file
    spawn_compress(dir, base_name, full, use_lz4, use_gz, max_files);
  }
  closedir(dp);
}

/* ---------------- rotation core ---------------- */

static bool open_active(io_log_t *h) {
  io_out_options_t o = h->opts;
  o.append_mode = true;          // append to active
  // ensure no compression on active file
  o.gz = false; o.lz4 = false;
  h->active = io_out_init(h->active_path, &o);
  h->bytes_written = 0;
  h->last_rotate = time(NULL);
  return (h->active != NULL);
}

static bool rotate_if_needed(io_log_t *h) {
  time_t now = time(NULL);
  bool time_hit = (h->rotate_interval && (now - h->last_rotate >= h->rotate_interval));
  bool size_hit = (h->rotate_size && (h->bytes_written >= h->rotate_size));
  if (!time_hit && !size_hit) return true;

  // close current active
  if (h->active) { io_out_destroy(h->active); h->active = NULL; }

  // rename active -> uncompressed rotated
  char rotated_uncompressed[768];
  rotated_name(rotated_uncompressed, sizeof(rotated_uncompressed), h->dir_path, h->base_name);
  rename(h->active_path, rotated_uncompressed);

  // spawn async compression for the rotated file
  spawn_compress(h->dir_path, h->base_name, rotated_uncompressed, h->use_lz4, h->use_gz, h->max_files);

  // reopen fresh active
  return open_active(h);
}

/* ---------------- core write helper ---------------- */

static bool write_with_header(io_log_t *h,
                              uint64_t ts,
                              bool have_ttl,
                              uint32_t ttl,
                              const void *data,
                              size_t len) {
  if (!h || !h->active) return false;
  size_t header = 8 + 4 + (have_ttl ? 4 : 0);
  size_t total = header + len;
  char *buf = malloc(total);
  if (!buf) return false;

  memcpy(buf, &ts, 8);
  if (have_ttl) memcpy(buf + 8 + 4, &ttl, 4);
  if (len) memcpy(buf + header, data, len);

  uint32_t h32 = lz4_hash32(buf + 8 + 4, total - (8 + 4));
  h32 = (h32 & ~1u) | (have_ttl ? 1u : 0u);
  memcpy(buf + 8, &h32, 4);

  bool ok = io_out_write_record(h->active, buf, total);
  free(buf);
  if (ok) {
    h->bytes_written += total;
  }
  return ok;
}

/* ---------------- public API ---------------- */

io_log_t *io_log_init(const char *base,
                      io_out_options_t *opts,
                      size_t rotate_size,
                      time_t rotate_interval,
                      size_t max_files) {
  io_log_t *h = (io_log_t *)aml_zalloc(sizeof(*h));
  h->base_path = strdup(base);
  split_dir_base(base, h->dir_path, sizeof(h->dir_path),
                 h->base_name, sizeof(h->base_name));

  h->opts = *opts;
  h->opts.format = 0;  // enforce prefix
  h->rotate_size = rotate_size;
  h->rotate_interval = rotate_interval;
  h->max_files = max_files;

  h->use_lz4 = opts->lz4;
  h->use_gz  = (!h->use_lz4 && opts->gz);

  char active_fname[512];
  snprintf(active_fname, sizeof(active_fname), "%s.active", h->base_name);
  make_path(h->active_path, sizeof(h->active_path), h->dir_path, active_fname);

  recover_active_auto(h->active_path);

  recompress_leftovers(h->dir_path, h->base_name, h->use_lz4, h->use_gz, h->max_files);

  if (!open_active(h)) {
    io_log_destroy(h);
    return NULL;
  }
  return h;
}

bool io_log_write(io_log_t *h, const void *data, size_t len) {
  return write_with_header(h, (uint64_t)time(NULL), false, 0, data, len);
}

bool io_log_write_ts(io_log_t *h, uint64_t ts, const void *data, size_t len) {
  return write_with_header(h, ts, false, 0, data, len);
}

bool io_log_write_ts_ttl(io_log_t *h, uint64_t ts, uint32_t ttl,
                         const void *data, size_t len) {
  return write_with_header(h, ts, true, ttl, data, len);
}

void io_log_flush(io_log_t *h) {
  if (h && h->active) {
    io_out_destroy(h->active);
    h->active = NULL;
  }
}

void io_log_destroy(io_log_t *h) {
  if (!h) return;
  io_log_flush(h);
  free(h->base_path);
  aml_free(h);
}
