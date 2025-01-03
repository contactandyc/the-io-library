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

#include "the-io-library/io.h"

#include "a-memory-library/aml_alloc.h"
#include "the-lz4-library/lz4.h"
#include "the-macro-library/macro_sort.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

_macro_sort_compare(io_sort_records, cmp_arg, io_record_t);

bool io_keep_first(io_record_t *res, const io_record_t *r,
                      size_t num_r, aml_buffer_t *bh, void *tag) {
  *res = *r;
  return true;
}

size_t io_hash_partition(const io_record_t *r, size_t num_part,
                            void *arg) {
  size_t offs = arg ? (*(size_t *)arg) : 0;
  uint64_t hash = lz4_hash64(r->record + offs, (r->length - offs) + 1);
  return hash % num_part;
}

bool io_extension(const char *filename, const char *extension) {
  if(!filename)
    return false;
  const char *r = strrchr(filename, '/');
  if (r)
    filename = r + 1;
  r = strrchr(filename, '.');
  if (!extension || extension[0] == 0)
    return r ? false : true;
  else if (!r)
    return false;
  return strcmp(r + 1, extension) ? false : true;
}

io_format_t io_delimiter(int delim) {
  delim++;
  return -delim;
}

io_format_t io_csv_delimiter(int delim) {
  delim += 257;
  return -delim;
}

io_format_t io_fixed(int size) { return size; }

io_format_t io_prefix() { return 0; }

bool io_make_directory(const char *path) {
  DIR *d = opendir(path);
  if (d)
    closedir(d);
  else {
    size_t cmd_len = 11 + strlen(path);
    char *cmd = (char *)aml_malloc(cmd_len);
    snprintf(cmd, cmd_len, "mkdir -p %s", path);
    if (system(cmd) != 0) {
      aml_free(cmd);
      return false;
    }
    aml_free(cmd);
  }
  chmod(path,
        S_IWUSR | S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
  return true;
}

bool io_make_path_valid(char *filename) {
  char *slash = strrchr(filename, '/');
  if (!slash)
    return true;
  *slash = 0;
  if (!io_make_directory(filename)) {
    *slash = '/';
    return false;
  }
  *slash = '/';
  return true;
}

uint64_t io_hash_filename(const char *filename) {
  return lz4_hash64(filename, strlen(filename));
}

bool io_file_info(io_file_info_t *fi) {
  if (!fi || !fi->filename)
    return false;
  struct stat sb;
  if (stat(fi->filename, &sb) == -1 || (sb.st_mode & S_IFMT) != S_IFREG)
    return false;
  fi->last_modified = sb.st_mtime;
  fi->size = sb.st_size;
  fi->tag = 0;
  return true;
}

bool io_directory(const char *filename) {
  if (!filename)
    return false;
  struct stat sb;
  if (stat(filename, &sb) == -1 || (sb.st_mode & S_IFMT) != S_IFDIR)
    return false;
  return true;
}

bool io_file(const char *filename) {
  if (!filename)
    return false;
  struct stat sb;
  if (stat(filename, &sb) == -1 || (sb.st_mode & S_IFMT) != S_IFREG)
    return false;
  return true;
}

time_t io_modified(const char *filename) {
  if (!filename)
    return 0;
  struct stat sb;
  if (stat(filename, &sb) == -1 || (sb.st_mode & S_IFMT) != S_IFREG)
    return 0;
  return sb.st_mtime;
}

size_t io_file_size(const char *filename) {
  if (!filename)
    return 0;
  struct stat sb;
  if (stat(filename, &sb) == -1 || (sb.st_mode & S_IFMT) != S_IFREG)
    return 0;
  return sb.st_size;
}

bool io_file_exists(const char *filename) {
  if (!filename)
    return false;
  struct stat sb;
  if (stat(filename, &sb) == -1 || (sb.st_mode & S_IFMT) != S_IFREG)
    return false;
  return true;
}

io_file_info_t *io_partition_file_info(aml_pool_t *pool, size_t *num_res,
                                       io_file_info_t *inputs,
                                       size_t num_inputs,
                                       size_t partition,
                                       size_t num_partitions,
                                       io_partition_file_cb partition_cb,
                                       void *tag) {
  io_file_info_t *p = inputs;
  io_file_info_t *ep = p + num_inputs;
  size_t num_matching = 0;
  while (p < ep) {
    if (partition_cb(p, num_partitions, tag) == partition)
      num_matching++;
    p++;
  }
  *num_res = num_matching;
  if (!num_matching)
    return NULL;
  p = inputs;
  io_file_info_t *res = (io_file_info_t *)aml_pool_alloc(
      pool, sizeof(io_file_info_t) * num_matching);
  io_file_info_t *wp = res;
  while (p < ep) {
    if (partition_cb(p, num_partitions, tag) == partition) {
      *wp = *p;
      wp++;
    }
    p++;
  }
  return res;
}

typedef struct io_file_info_link_s {
  io_file_info_t fi;
  struct io_file_info_link_s *next;
} io_file_info_link_t;

typedef struct {
  io_file_info_link_t *head;
  io_file_info_link_t *tail;
  size_t num_files;
  size_t bytes;
} io_file_info_root_t;

void _io_list(io_file_info_root_t *root, const char *path,
                 aml_pool_t *pool, io_file_valid_cb file_valid, void *arg) {
  if (!path)
    path = "";
  DIR *dp = opendir(path[0] ? path : ".");
  if (!dp)
    return;

  char *filename = (char *)aml_malloc(8192);
  struct dirent *entry;

  while ((entry = readdir(dp)) != NULL) {
    if (entry->d_name[0] == '.')
      continue;
    snprintf(filename, 8192, "%s/%s", path, entry->d_name);
    io_file_info_t fi;
    fi.filename = filename;
    if (!io_file_info(&fi)) {
      if (io_directory(filename))
        _io_list(root, filename, pool, file_valid, arg);
    } else {
      if (!file_valid || file_valid(filename, arg)) {
        size_t len = strlen(filename) + 1 + sizeof(io_file_info_link_t);
        io_file_info_link_t *n;
        if (pool)
          n = (io_file_info_link_t *)aml_pool_alloc(pool, len);
        else
          n = (io_file_info_link_t *)aml_malloc(len);
        n->fi.filename = (char *)(n + 1);
        n->next = NULL;
        strcpy(n->fi.filename, filename);
        io_file_info(&(n->fi));
        if (!root->head)
          root->head = root->tail = n;
        else {
          root->tail->next = n;
          root->tail = n;
        }
        root->bytes += strlen(filename) + 1;
        root->num_files++;
      }
    }
  }
  aml_free(filename);
  (void)closedir(dp);
}

static io_file_info_t *
__io_list(aml_pool_t *pool, const char *path, size_t *num_files,
             io_file_valid_cb file_valid, void *arg, const char *caller) {
  io_file_info_root_t root;
  root.head = root.tail = NULL;
  root.num_files = root.bytes = 0;
  aml_pool_t *tmp_pool = aml_pool_init(4096);
  _io_list(&root, path, tmp_pool, file_valid, arg);
  *num_files = root.num_files;
  if (!root.num_files) {
    aml_pool_destroy(tmp_pool);
    return NULL;
  }
  io_file_info_t *res;
  if (pool)
    res = (io_file_info_t *)aml_pool_zalloc(
        pool, (sizeof(io_file_info_t) * root.num_files) + root.bytes);
  else {
    if(caller) {
        res = (io_file_info_t *)_aml_malloc_d(caller, (sizeof(io_file_info_t) * root.num_files) + root.bytes, false);
        memset(res, 0, (sizeof(io_file_info_t) * root.num_files) + root.bytes);
    }
    else
        res = (io_file_info_t *)aml_zalloc(
            (sizeof(io_file_info_t) * root.num_files) + root.bytes);
  }
  char *mem = (char *)(res + root.num_files);
  io_file_info_t *rp = res;
  io_file_info_link_t *n = root.head;
  while (n) {
    *rp = n->fi;
    rp->filename = mem;
    strcpy(rp->filename, n->fi.filename);
    mem += strlen(rp->filename) + 1;
    rp++;
    n = n->next;
  }
  aml_pool_destroy(tmp_pool);
  return res;
}


#ifdef _AML_DEBUG_
io_file_info_t *
io_list_d(const char *path, size_t *num_files,
           io_file_valid_cb file_valid, void *arg, const char *caller) {
  return __io_list(NULL, path, num_files, file_valid, arg, caller);
}
#else
io_file_info_t *
io_list_d(const char *path, size_t *num_files,
           io_file_valid_cb file_valid, void *arg) {
  return __io_list(NULL, path, num_files, file_valid, arg, NULL);
}
#endif

io_file_info_t *
io_pool_list(aml_pool_t *pool, const char *path, size_t *num_files,
             io_file_valid_cb file_valid, void *arg) {
  return __io_list(pool, path, num_files, file_valid, arg, NULL);
}

static inline
bool compare_io_file_info_last_modified(const io_file_info_t *a, const io_file_info_t *b) {
    return a->last_modified < b->last_modified;
}

static macro_sort(_sort_io_file_info_last_modified, io_file_info_t, compare_io_file_info_last_modified);

void io_sort_file_info_by_last_modified(io_file_info_t *files, size_t num_files) {
    _sort_io_file_info_last_modified(files, num_files);
}

static inline
bool compare_io_file_info_last_modified_descending(const io_file_info_t *a, const io_file_info_t *b) {
    return a->last_modified < b->last_modified ? 1 : -1; // descending
}

static macro_sort(_sort_io_file_info_last_modified_descending, io_file_info_t, compare_io_file_info_last_modified_descending);

void io_sort_file_info_by_last_modified_descending(io_file_info_t *files, size_t num_files) {
    _sort_io_file_info_last_modified_descending(files, num_files);
}

static inline
bool compare_io_file_info_size(const io_file_info_t *a, const io_file_info_t *b) {
    return a->size < b->size;
}

static macro_sort(_sort_io_file_info_size, io_file_info_t, compare_io_file_info_size);

void io_sort_file_info_by_size(io_file_info_t *files, size_t num_files) {
    _sort_io_file_info_size(files, num_files);
}

static inline
bool compare_io_file_info_size_descending(const io_file_info_t *a, const io_file_info_t *b) {
    return a->size < b->size ? 1 : -1; // descending
}

static macro_sort(_sort_io_file_info_size_descending, io_file_info_t, compare_io_file_info_size_descending);

void io_sort_file_info_by_size_descending(io_file_info_t *files, size_t num_files) {
    _sort_io_file_info_size_descending(files, num_files);
}


static inline
bool compare_io_file_info_filename(const io_file_info_t *a, const io_file_info_t *b) {
    return strcmp(a->filename, b->filename);
}

static macro_sort(_sort_io_file_info_filename, io_file_info_t, compare_io_file_info_filename);

void io_sort_file_info_by_filename(io_file_info_t *files, size_t num_files) {
    _sort_io_file_info_filename(files, num_files);
}

static inline
bool compare_io_file_info_filename_descending(const io_file_info_t *a, const io_file_info_t *b) {
    return strcmp(b->filename, a->filename); // descending
}

static macro_sort(_sort_io_file_info_filename_descending, io_file_info_t, compare_io_file_info_filename_descending);

void io_sort_file_info_by_filename_descending(io_file_info_t *files, size_t num_files) {
    _sort_io_file_info_filename_descending(files, num_files);
}

char *io_pool_read_file(aml_pool_t *pool, size_t *len, const char *filename) {
    *len = 0;
    if (!filename)
        return NULL;

    int fd = open(filename, O_RDONLY);
    if (fd == -1)
        return NULL;

    size_t length = io_file_size(filename);
    char *buf = (char *)aml_pool_alloc(pool, length + 1);
    if (buf) {
        size_t s = length;
        size_t pos = 0;
        size_t chunk = 64 * 1024 * 1024;
        while (s > chunk) {
            if (read(fd, buf + pos, chunk) != chunk) {
                close(fd);
                return NULL;
            }
            pos += chunk;
            s -= chunk;
        }
        if (read(fd, buf + pos, s) == (ssize_t)s) {
            close(fd);
            buf[length] = 0;
            *len = length;
            return buf;
        }
    }
    close(fd);
    return NULL;
}

#ifdef _AML_DEBUG_
char *_io_read_file(size_t *len, const char *filename, const char *caller) {
#else
char *_io_read_file(size_t *len, const char *filename) {
#endif
  *len = 0;
  if (!filename)
    return NULL;

  int fd = open(filename, O_RDONLY);
  if (fd == -1)
    return NULL;

  size_t length = io_file_size(filename);
  if (length) {
#ifdef _AML_DEBUG_
    char *buf = (char *)_aml_malloc_d(caller, length + 1, false);
#else
    char *buf = (char *)aml_malloc(length + 1);
#endif
    if (buf) {
      size_t s = length;
      size_t pos = 0;
      size_t chunk = 64 * 1024 * 1024;
      while (s > chunk) {
        if (read(fd, buf + pos, chunk) != chunk) {
          aml_free(buf);
          close(fd);
          return NULL;
        }
        pos += chunk;
        s -= chunk;
      }
      if (read(fd, buf + pos, s) == (ssize_t)s) {
        close(fd);
        buf[length] = 0;
        *len = length;
        return buf;
      }
      aml_free(buf);
      buf = NULL;
    }
  }
  close(fd);
  return NULL;
}
