// SPDX-FileCopyrightText: 2019–2025 Andy Curtis <contactandyc@gmail.com>
// SPDX-FileCopyrightText: 2024–2025 Knode.ai — technical questions: contact Andy (above)
// SPDX-License-Identifier: Apache-2.0

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
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096  // Define a reasonable fallback if PATH_MAX is not defined
#endif

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

io_format_t io_prefix(void) { return 0; }

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

char *io_find_file_in_parents(const char *path) {
  if (!path)
    return NULL;

  char cwd[PATH_MAX];
  if (!getcwd(cwd, sizeof(cwd)))
    return NULL;

  struct stat sb;
  char fullpath[PATH_MAX];

  while (1) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(fullpath, sizeof(fullpath), "%s/%s", cwd, path);
#pragma GCC diagnostic pop
    if (stat(fullpath, &sb) == 0)
      return aml_strdup(fullpath); // Found, return dynamically allocated string

    // Move up one directory
    char *last_slash = strrchr(cwd, '/');
    if (!last_slash || last_slash == cwd) // Stop at root "/"
      break;

    *last_slash = '\0'; // Trim last directory
  }

  return NULL; // Not found
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
#ifdef _AML_DEBUG_
    res = (io_file_info_t *)_aml_malloc_d(caller, (sizeof(io_file_info_t) * root.num_files) + root.bytes, false);
    memset(res, 0, (sizeof(io_file_info_t) * root.num_files) + root.bytes);
#else
    res = (io_file_info_t *)aml_zalloc(
        (sizeof(io_file_info_t) * root.num_files) + root.bytes);
#endif
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
    char *buf = (char *)aml_pool_aalloc(pool, 64, length + 1);
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

bool io_read_chunk_into_buffer(char *buffer, size_t *len,
                               const char *filename, size_t offset, size_t length)
{
    if (!filename || !buffer || length == 0) {
        if (len)
            *len = 0;
        return false;
    }

    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        if (len)
            *len = 0;
        return false;
    }

    // Seek to the specified offset if needed.
    if (offset > 0) {
        off_t ret = lseek(fd, (off_t)offset, SEEK_SET);
        if (ret == (off_t)-1) {
            close(fd);
            if (len)
                *len = 0;
            return false;
        }
    }

    size_t bytes_remaining = length;
    size_t pos = 0;
    const size_t CHUNK = 64UL * 1024UL * 1024UL;  /* 64 MB */

    // Read the file in CHUNK-sized blocks.
    while (bytes_remaining > CHUNK) {
        ssize_t r = read(fd, buffer + pos, CHUNK);
        if (r != (ssize_t)CHUNK) {
            close(fd);
            if (len)
                *len = pos;
            return false;
        }
        pos += CHUNK;
        bytes_remaining -= CHUNK;
    }

    // Read the final partial chunk.
    ssize_t r = read(fd, buffer + pos, bytes_remaining);
    if (r != (ssize_t)bytes_remaining) {
        close(fd);
        if (len)
            *len = pos;
        return false;
    }

    pos += bytes_remaining;
    close(fd);
    if (len)
        *len = pos;
    return true;
}

char *io_pool_read_chunk(aml_pool_t *pool, size_t *len,
                         const char *filename, size_t offset, size_t length)
{
    *len = 0;
    if (!filename || length == 0) {
        return NULL;
    }

    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        /* Failed to open the file */
        return NULL;
    }

    /* Seek to offset if requested */
    if (offset > 0) {
        off_t ret = lseek(fd, (off_t)offset, SEEK_SET);
        if (ret == (off_t)-1) {
            /* Seek failed */
            close(fd);
            return NULL;
        }
    }

    /* Allocate space for 'length' bytes */
    char *buf = (char *)aml_pool_aalloc(pool, 64, length);
    if (!buf) {
        close(fd);
        return NULL;
    }

    size_t bytes_remaining = length;
    size_t pos = 0;
    /* Same chunk logic as in io_pool_read_file */
    const size_t CHUNK = 64UL * 1024UL * 1024UL;  /* 64 MB */

    /* Read in chunks until we've read 'length' bytes or fail */
    while (bytes_remaining > CHUNK) {
        ssize_t r = read(fd, buf + pos, CHUNK);
        if (r != (ssize_t)CHUNK) {
            /* Something went wrong or EOF reached prematurely */
            close(fd);
            return NULL;
        }
        pos += CHUNK;
        bytes_remaining -= CHUNK;
    }

    /* Read the final partial chunk (which could be smaller or 0 if length was multiple of CHUNK) */
    if (read(fd, buf + pos, bytes_remaining) == (ssize_t)bytes_remaining) {
        close(fd);
        *len = length;
        return buf;
    }

    /* If we get here, the last read didn't match bytes_remaining */
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

#ifdef _AML_DEBUG_
char *_io_read_chunk(size_t *len, const char *filename, size_t offset, size_t length, const char *caller) {
#else
char *_io_read_chunk(size_t *len, const char *filename, size_t offset, size_t length) {
#endif
  /* Initialize *len = 0 in case of failure */
  *len = 0;

  /* Basic sanity checks */
  if (!filename || !length) {
    return NULL;
  }

  int fd = open(filename, O_RDONLY);
  if (fd == -1) {
    /* Could not open the file */
    return NULL;
  }

  /* If offset > 0, seek to that position in the file */
  if (offset > 0) {
    off_t ret = lseek(fd, (off_t)offset, SEEK_SET);
    if (ret == (off_t)-1) {
      /* Seek failed */
      close(fd);
      return NULL;
    }
  }

  /* Allocate a buffer of length+1 for a null terminator */
#ifdef _AML_DEBUG_
  char *buf = (char *)_aml_malloc_d(caller, length + 1, false);
#else
  char *buf = (char *)aml_malloc(length + 1);
#endif

  if (!buf) {
    close(fd);
    return NULL;
  }

  /* Read the data in a loop until we've read `length` or hit EOF/error */
  size_t total_read = 0;
  while (total_read < length) {
    ssize_t r = read(fd, buf + total_read, length - total_read);
    if (r < 0) {
      /* Read error */
      aml_free(buf);
      close(fd);
      return NULL;
    } else if (r == 0) {
      /* Reached EOF */
      break;
    }
    total_read += (size_t)r;
  }

  /* We no longer need the file descriptor */
  close(fd);

  /* If we couldn’t read anything at all, return NULL (or handle differently) */
  if (total_read == 0) {
    aml_free(buf);
    return NULL;
  }

  /* Null-terminate the buffer; total_read ≤ length */
  buf[total_read] = '\0';

  /* Set the output parameter to number of bytes actually read */
  *len = total_read;

  return buf;
}

char *io_read_file_aligned(size_t *len, size_t alignment, const char *filename) {
  *len = 0;
  if (!filename)
    return NULL;

  int fd = open(filename, O_RDONLY);
  if (fd == -1)
    return NULL;

  size_t length = io_file_size(filename);
  if((length % alignment) != 0) {
    fprintf(stderr, "Error: File size is not a multiple of the alignment\n");
    return NULL;
  }
  if (length) {
    char *buf = (char *)aligned_alloc(alignment, length);
    if (buf) {
      size_t s = length;
      size_t pos = 0;
      size_t chunk = 16 * 1024 * 1024;
      while (s > chunk) {
        if (read(fd, buf + pos, chunk) != chunk) {
          free(buf);
          close(fd);
          return NULL;
        }
        pos += chunk;
        s -= chunk;
      }
      if (read(fd, buf + pos, s) == (ssize_t)s) {
        close(fd);
        *len = length;
        return buf;
      }
      free(buf);
      buf = NULL;
    }
  }
  close(fd);
  return NULL;
}
