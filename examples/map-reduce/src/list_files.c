// SPDX-FileCopyrightText: 2019–2025 Andy Curtis <contactandyc@gmail.com>
// SPDX-FileCopyrightText: 2024–2025 Knode.ai — technical questions: contact Andy (above)
// SPDX-License-Identifier: Apache-2.0

#include "a-memory-library/aml_alloc.h"
#include "a-memory-library/aml_pool.h"
#include "the-io-library/io.h"
#include "the-macro-library/macro_to.h"

#include <stdio.h>
#include <locale.h>

#include <time.h>

bool file_ok(const char *filename, void *arg) {
  char **extensions = (char **)arg;
  char **p = extensions;
  while (*p) {
    if (io_extension(filename, *p))
      return true;
    p++;
  }
  return false;
}

int usage(const char *prog) {
  printf("%s <extensions> <path> [path2] ...\n", prog);
  printf("extensions - a comma delimited list of valid extensions\n");
  printf("\n");
  return 0;
}

int main(int argc, char *argv[]) {
    setlocale(LC_NUMERIC, "");

    if (argc < 3)
        return usage(argv[0]);

    const char *ext = argv[1];

    aml_pool_t *pool = aml_pool_init(1024);
    size_t num_extensions = 0;
    char **extensions = aml_pool_split2(pool, &num_extensions, ',', ext);

    size_t total = 0;
    size_t total_files = 0;
    for( int i=2; i<argc; i++ ) {
        const char *path = argv[i];
        size_t num_files = 0;
        io_file_info_t *files = io_list(path, &num_files, file_ok, extensions);
        total_files += num_files;
        char date_time[20];

        for (size_t i = 0; i < num_files; i++) {
            total += files[i].size;
            printf("%s %'20lu\t%s\n", macro_to_date_time(date_time, files[i].last_modified),
                   files[i].size, files[i].filename);
        }
        // aml_free(files);
    }
    printf("%'lu byte(s) in %'lu file(s)\n", total, total_files);
    aml_pool_destroy(pool);
    return 0;
}
