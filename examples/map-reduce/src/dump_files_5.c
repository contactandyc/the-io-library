// SPDX-FileCopyrightText: 2019–2025 Andy Curtis <contactandyc@gmail.com>
// SPDX-FileCopyrightText: 2024–2025 Knode.ai — technical questions: contact Andy (above)
// SPDX-License-Identifier: Apache-2.0

#include "a-memory-library/aml_alloc.h"
#include "a-memory-library/aml_pool.h"
#include "the-io-library/io.h"
#include "the-io-library/io_in.h"
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

int compare_strings(const io_record_t *a, const io_record_t *b, void *tag __attribute__((unused))) {
    int n=strcmp(a->record, b->record);
    if(n) return n;
    return a->tag-b->tag;
}

int main(int argc, char *argv[]) {
    setlocale(LC_NUMERIC, "");

    if (argc < 3)
        return usage(argv[0]);

    const char *ext = argv[1];

    aml_pool_t *pool = aml_pool_init(1024);
    size_t num_extensions = 0;
    char **extensions = aml_pool_split2(pool, &num_extensions, ',', ext);

    io_in_options_t opts;
    io_in_options_init(&opts);
    io_in_options_format(&opts, io_delimiter('\n'));

    io_in_t *in = io_in_ext_init(compare_strings, NULL, &opts);
    io_in_ext_keep_first(in);

    for( int i=2; i<argc; i++ ) {
        const char *path = argv[i];
        size_t num_files = 0;
        io_file_info_t *files = io_list(path, &num_files, file_ok, extensions);

        for( size_t j=0; j<num_files; j++ )
            io_in_ext_add(in, io_in_init(files[j].filename, &opts), (i*1000)+j);

        aml_free(files);
    }
    io_record_t *r;
    while ((r = io_in_advance(in)) != NULL)
        printf("%d: %s\n", r->tag, r->record);

    io_in_destroy(in);
    aml_pool_destroy(pool);
    return 0;
}
