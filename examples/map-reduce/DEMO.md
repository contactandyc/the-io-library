The IO library consists of a number of ways to transform data.  

## Listing files

io is a collection of useful input/output related functions and structures.  `io_list` will find all of the files within a directory that matches your criteria.  Each file's name, size, and last modified time stamp will be returned to you.  The tag is meant for other purposes which will be explored later.  `io_list` will return an array of `io_file_info_t` structures which are defined as follows.

in `include/the-io-library/io.h`
```c
struct io_file_info_t {
  char *filename;
  size_t size;
  time_t last_modified;
  int32_t tag;
};
```

and here's the prototype for `io_list`...
```c
io_file_info_t *io_list(const char *path, size_t *num_files,
                        bool (*file_valid)(const char *filename,
                                          void *arg),
                        void *arg);
```

`io_list` will return an array of `io_file_info_t` which has `num_files` elements (set by `io_list`) from the given path.  The `file_valid` is a user callback that if provided can return true or false to indicate whether the file should be returned in the list.  The arg is passed through from `io_list` call to the callback `file_valid`.

To follow along with code in the terminal, you will need to be in the examples/map-reduce directory.

`examples/map-reduce/src/list_files.c`:
```c
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
        if(files) {
            total_files += num_files;
            char date_time[20];
    
            for (size_t i = 0; i < num_files; i++) {
                total += files[i].size;
                printf("%s %'20lu\t%s\n", macro_to_date_time(date_time, files[i].last_modified),
                       files[i].size, files[i].filename);
            }
            // aml_free(files);
        }
    }
    printf("%'lu byte(s) in %'lu file(s)\n", total, total_files);
    aml_pool_destroy(pool);
    return 0;
}
```

```bash
$ mkdir -p build
$ cd build
$ cmake ..
$ make list_files
$ ./list_files c,h ../../../src ../../../include          
2023-11-30 21:01:52               12,059	../../../src/io.c
2023-11-30 13:31:43               11,245	../../../src/io_in_base.c
2023-11-30 13:33:50               44,300	../../../src/io_in.c
2023-11-30 13:26:02               43,685	../../../src/io_out.c
2023-11-30 13:53:11                1,346	../../../include/the-io-library/io_in_base.h
2023-11-30 13:53:27                6,850	../../../include/the-io-library/io_out.h
2023-11-30 13:53:24               12,323	../../../include/the-io-library/io_in.h
2023-11-30 20:59:16               10,165	../../../include/the-io-library/io.h
2023-11-30 13:45:19                  747	../../../include/the-io-library/src/io_in_base.h
2023-11-30 13:46:09                1,535	../../../include/the-io-library/src/io_out.h
2023-11-30 13:44:55                1,082	../../../include/the-io-library/src/io_in.h
145,337 byte(s) in 11 file(s)
740 byte(s) allocated in 2 allocations (80 byte(s) overhead)
... /the-io-library/examples/map-reduce/src/list_files.c:46 [io_list]: 215 
... /the-io-library/examples/map-reduce/src/list_files.c:46 [io_list]: 525 
```

A few quick C things to call out about the example above.  The first line of the main function calls setlocale.
```c
setlocale(LC_NUMERIC, "");
```

This causes printf and %' to print comma delimited numbers.  setlocale is found in locale.h.

Because all of the files have a `time_t` time stamp, I've included `macro_to.h` to use the `macro_to_date_time` method to convert the time stamp into a readable format.  `macro_to_date_time` requires a 20 byte array to passed to it and returns a reference to it so it can be used inline (as it is in the printf call above).

`aml_pool.h` is included because it has a function to split strings like the comma delimited extensions list.  The `aml_pool` manages its own memory, so there is no need to free or release the memory manually from the `aml_pool_split2` call.  The `aml_pool_split2` function splits the string into an array and removes any blank entries. 

In file_ok...
```c
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
```

`io_extension` is found in `io.h` and returns true if the given string matches the given extension.  If the string is an empty string "", then it will match a file if no extension exists (there is no way to match a file that ends in a period).

```c
size_t num_files = 0;
io_file_info_t *files = io_list(path, &num_files, file_ok, extensions);
```
Finds all of the files which have the expected extensions.

`io_list` allocates memory using `aml_malloc` and must be freed using `aml_free`.  Notice above that when the program exits, it indicates that there is two memory leaks at line 46 of `list_files.c`.  If the `// aml_free(files);` was uncommented, this error would go away.

## Reading a file a line at a time

The `io_in` object allows reading a file at a time, merging a group of sorted files as if the file is a single file, creating a stream from a buffer, a set of records, and more.  It also allows reading a gzip or lz4 file as if it was uncompressed.

The `dump_files_1.c` dumps all of the files that were listed using `list_files` above. 

The new code is below

1. Setup `io_in_options_t`.  This defines the format of the input file.  The files are text, so the options are set to treat each line as a record.
```c
io_in_options_t opts;
io_in_options_init(&opts);
io_in_options_format(&opts, io_delimiter('\n'));
```

2. For each file, call `io_in_init` to open the file for reading, `io_in_advance` to advance through each record, and finally `io_in_destroy` to close the file and release resources.
```c
for (size_t i = 0; i < num_files; i++) {
    io_record_t *r;
    io_in_t *in = io_in_init(files[i].filename, &opts);
    while ((r = io_in_advance(in)) != NULL)
        printf("%s\n", r->record);
    io_in_destroy(in);
}
```

```
$ make dump_files_1
$ ./dump_files_1 txt ../sample_files
This is line 1
This is line 2
This is line 3
$ ./dump_files_1 lz4 ../sample_files
This is line 1
This is line 2
This is line 3
$ ./dump_files_1 gz ../sample_files
This is line 1
This is line 2
This is line 3
```

The example above dumps the contents of sample.txt, sample.txt.gz, and sample.txt.lz4.  `io_in` automatically understands files that have a gz or lz4 extension to be compressed.

The `io_in` object is configured so to understand what a record looks like.  In this case, a record is a line of text and each line is delimited by a newline character `\n`.  The `io_in_options_t` structure is first initialized and then the record type is indicated.

```c
io_in_options_t opts;
io_in_options_init(&opts);
io_in_options_format(&opts, io_io_delimiter('\n'));
```

The for loop after it will open each file as an `io_in` object and then print each record.

```c
for (size_t i = 0; i < num_files; i++) {
    io_record_t *r;
    io_in_t *in = io_in_init(files[i].filename, &opts);
    while ((r = io_in_advance(in)) != NULL)
        printf("%s\n", r->record);
    io_in_destroy(in);
}
```

### Using the io_in object to scan multiple files

This code can be further simplified using `io_in_init_from_list`.  This will cause the `io_in` object to internally handle opening and closing files.  In this case, only one file is open at a time and they are in the order of the `files` list.

`src/dump_files_2.c`:
```c
io_in_t *in = io_in_init_from_list(files, num_files, &opts);
io_io_record_t *r;
while ((r = io_in_advance(in)) != NULL)
  printf("%s\n", r->record);
io_in_destroy(in);
```

The sample files are all the same and they are sorted alphanumerically.  `io_in_ext_init` can be used to open multiple files and then read them as one.  This is particularly useful if the files are sorted and a final sorted output is desired.

### Iterating over a set of sorted files as if they are one

`src/dump_files_3.c`:
1.  Define a compare function to define the order of the records 
```c
int compare_strings(const io_record_t *a, const io_record_t *b, void *tag) {
    return strcmp(a->record, b->record);
}
```

2. Setup the `io_in` object using `io_in_ext_init`.
```c
    io_in_options_t opts;
    io_in_options_init(&opts);
    io_in_options_format(&opts, io_delimiter('\n'));

    io_in_t *in = io_in_ext_init(compare_strings, NULL, &opts);
```

3.  Add the files to `in` using `io_in_ext_add`.
```c
    for( int i=2; i<argc; i++ ) {
        const char *path = argv[i];
        size_t num_files = 0;
        io_file_info_t *files = io_list(path, &num_files, file_ok, extensions);

        for( size_t j=0; j<num_files; j++ )
            io_in_ext_add(in, io_in_init(files[j].filename, &opts), (i*1000)+j);

        aml_free(files);
    }
```

4.  advance over the `io_in` object just as if it is a single file
```c
    io_record_t *r;
    while ((r = io_in_advance(in)) != NULL)
        printf("%d: %s\n", r->tag, r->record);
```

5.  Destroy `in`
```c
    io_in_destroy(in);
```

```bash
$ make dump_files_3
$ ./dump_files_3 txt,gz ../sample_files
2000: This is line 1
2001: This is line 1
2001: This is line 2
2000: This is line 2
2001: This is line 3
2000: This is line 3
```

Notice that all of `This is line 1` precedes all `This is line 2`.  Also notice that the tag that is prefixed is out of order.  The last parameter to `io_in_ext_add` is a numeric tag that gets set for all records for the given input.

To fix this, the `compare_strings` can be updated to consider the `tag`.

`src/dump_files_4.c`:
```c
int compare_strings(const io_record_t *a, const io_record_t *b, void *tag) {
    int n=strcmp(a->record, b->record);
    if(n) return n;
    return a->tag-b->tag;
}
```

```bash
$ make dump_files_4
$ ./dump_files_4 txt,gz ../sample_files
2000: This is line 1
2001: This is line 1
2000: This is line 2
2001: This is line 2
2000: This is line 3
2001: This is line 3
```

By adding the tag as a second order comparison, the order is stabilized.

# Reducer keep_first

`ac_in` also allows for reducing similar records.  `io_in_ext_reducer` allows for custom reduction.  `io_in_ext_keep_first` will keep the first record within a group of equal records.

`src/dump_files_5.c`:
```c
io_in_ext_keep_first(in);
```

```bash
$ make dump_files_5
$ ./dump_files_5 txt,gz ../sample_files 
2000: This is line 1
2001: This is line 1
2000: This is line 2
2001: This is line 2
2000: This is line 3
2001: This is line 3
```

No reduction happened!  This is because the compare_strings method makes all records unique.  If compare_strings is reverted so that it only compares the strings, it should work.

`src/dump_files_6.c`:
```c
int compare_strings(const io_record_t *a, const io_record_t *b, void *tag) {
    return strcmp(a->record, b->record);
}
```

```bash
$ make dump_files_6
$ ./dump_files_6 txt,gz ../sample_files 
2000: This is line 1
2001: This is line 2
2000: This is line 3
```

### Custom Reducer

Note that this time, only one record is present from each file, but in the second case, the second file is used.  Generally, this should not make a difference.  A custom reducer could fix this.

`src/dump_files_7.c`:
1.  Create the custom reducer.  The buffer `bh` can be used to create a new record.  The reducer does not have to return one of the input records.
```c
bool custom_keep_first(io_record_t *res, const io_record_t *r,
                       size_t num_r, aml_buffer_t *bh, void *tag) {
    size_t min_i = 0;
    int min_tag = r[0].tag;
    
    for(size_t i=1; i<num_r; i++ ) {
        if(r[i].tag < min_tag) {
            min_tag = r[i].tag;
            min_i = i;
        }
    }
    *res = r[min_i];
    return true;
}
```

2. Use the custom reducer
```c
io_in_ext_reducer(in, custom_keep_first, NULL);
```

```bash
$ make dump_files_7
$ ./dump_files_7 txt,gz ../sample_files 
2000: This is line 1
2000: This is line 2
2000: This is line 3
```

## To Be Continued!

