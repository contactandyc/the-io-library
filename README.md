# Overview of The IO Library

The IO Library, a comprehensive and versatile C library, is designed for efficient data input/output operations, file management, and data processing. It integrates seamlessly with the A Memory Library and The Macro Library, providing robust and high-performance solutions for handling various file formats and data manipulation tasks.

See [map-reduce demo](examples/map-reduce/DEMO.md) for a more detailed examples.

## Core Features of The IO Library

### File Format Handling
- **io_delimiter**: Specifies a delimiter for record separation in files.
- **io_csv_delimiter**: Defines a CSV delimiter for parsing CSV files.
- **io_fixed**: Sets a fixed size for records in a file.
- **io_prefix**: Handles variable-length records with a 4 byte prefix indicating size.

### Data Records
- **io_record_t**: A structure representing a data record, including content, length, and an optional tag for additional information.

### Data Processing Functions
- **io_reducer_cb**: A callback for consolidating multiple records into one.
- **io_compare_cb**: A comparison function for sorting and comparing records.
- **io_partition_cb**: Function for assigning records to partitions based on custom criteria.
- **io_fixed_reducer_cb**: Reduces fixed-size records.
- **io_fixed_sort_cb**: In-memory sorting function for fixed-size records.
- **io_fixed_compare_cb**: Compares fixed-size records.

### File Information and Management
- **io_file_info_t**: Stores information about files including name, size, last modification time, and a custom tag.
- **io_list**: Lists files in a directory based on custom criteria.
- **io_pool_list**: Similar to `io_list`, but uses a memory pool for allocation.
- **io_partition_file_info**: Selects files matching a specified partition.
- **io_sort_file_info_by_last_modified**: Sorts files based on their last modified time.
- **io_file_exists**, **io_file_size**, **io_modified**: Functions to check the existence, size, and last modified time of files.
- **io_directory**, **io_file**: Checks if a path is a directory or a file.
- **io_read_file**: Reads the contents of a file into a buffer.
- **io_make_directory**: Creates a directory.
- **io_make_path_valid**: Validates and creates the necessary path for a file.
- **io_extension**: Checks if a file has a specific extension.

### Record Partitioning and Comparison Utilities
- **io_compare_uint32_t**, **io_compare_uint64_t**: Compares records based on the first 32 or 64 bits.
- **io_split_by_uint32_t**, **io_split_by_uint64_t**: Partitions records based on the first 32 or 64 bits.
- **io_split_by_uint32_t_2**, **io_split_by_uint64_t_2**: Partitions records based on the second 32 or 64 bits.

The IO Library offers a range of functionalities that make it ideal for applications requiring sophisticated file handling and data processing capabilities. Its modular design, coupled with its integration with other libraries, ensures efficient and scalable solutions for a wide range of I/O operations.

## Dependencies

* [A cmake library](https://github.com/knode-ai-open-source/a-cmake-library) needed for the cmake build
* [A memory library](https://github.com/knode-ai-open-source/a-memory-library) for the memory handling
* [The macro library](https://github.com/knode-ai-open-source/the-macro-library) for sorting, searching, and conversions
* [The LZ4 Library](https://github.com/knode-ai-open-source/the-lz4-library) for compression
