// SPDX-FileCopyrightText: 2019–2025 Andy Curtis <contactandyc@gmail.com>
// SPDX-FileCopyrightText: 2024–2025 Knode.ai — technical questions: contact Andy (above)
// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include "the-io-library/io_data_store.h"
#include "the-io-library/io.h"

// FNV-1a 24-bit hash function
uint32_t fnv1a_24(const char *key) {
    const uint32_t FNV_PRIME = 16777619;
    const uint32_t OFFSET_BASIS = 2166136261;

    uint32_t hash = OFFSET_BASIS;
    while (*key) {
        hash ^= (uint8_t)(*key);
        hash *= FNV_PRIME;
        key++;
    }
    return hash & 0xFFFFFF;
}

// Internal helper: Creates a directory recursively
static bool create_directory(const char *path) {
    char temp[256];
    snprintf(temp, sizeof(temp), "%s", path);
    for (char *p = temp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
                return false;
            }
            *p = '/';
        }
    }
    return mkdir(temp, 0755) == 0 || errno == EEXIST;
}

// Internal helper: Generate file path from filename and base path
static void generate_file_path(char *buffer, size_t buffer_size, const char *base_path, const char *filename) {
    uint32_t hash = fnv1a_24(filename);
    snprintf(buffer, buffer_size, "%s/%03x/%03x/%03x/%03x/%s", base_path,
             (hash >> 18) & 0x3F, (hash >> 12) & 0x3F, (hash >> 6) & 0x3F, hash & 0x3F, filename);
}

// Structure definitions
struct io_data_store_s {
    char base_path[512];
};

struct io_data_store_cursor_s {
    DIR *dirs[4];
    char current_path[512];
};

io_data_store_t *io_data_store_init(const char *path) {
    io_data_store_t *store = aml_malloc(sizeof(io_data_store_t));
    if (!store) return NULL;
    snprintf(store->base_path, sizeof(store->base_path), "%s", path);
    create_directory(path);
    return store;
}

void io_data_store_destroy(io_data_store_t *h) {
    aml_free(h);
}

// Check if a file exists
bool io_data_store_exists(io_data_store_t *h, const char *filename) {
    char file_path[512];
    generate_file_path(file_path, sizeof(file_path), h->base_path, filename);
    return access(file_path, F_OK) == 0;
}

// Read a file into a buffer
char *io_data_store_read_file(io_data_store_t *h, size_t *file_length, const char *filename) {
    char file_path[512];
    generate_file_path(file_path, sizeof(file_path), h->base_path, filename);

    return io_read_file(file_length, file_path);
}

char *io_data_store_pool_read_file(io_data_store_t *h, aml_pool_t *pool, size_t *file_length, const char *filename) {
    char file_path[512];
    generate_file_path(file_path, sizeof(file_path), h->base_path, filename);

    return io_pool_read_file(pool, file_length, file_path);
}

// Write a file atomically
void io_data_store_write_file(io_data_store_t *h, const char *filename, const char *data, size_t data_length,
                              uint32_t temp_id) {
    char file_path[512];
    generate_file_path(file_path, sizeof(file_path), h->base_path, filename);
    if(!io_data_store_exists(h, filename)) {
        create_directory(file_path); // Ensure directories exist
    }

    char temp_path[128];
    snprintf(temp_path, sizeof(temp_path), ".data_store_tmp.%u", temp_id);

    FILE *temp_file = fopen(temp_path, "wb");
    if (!temp_file) return;

    fwrite(data, 1, data_length, temp_file);
    fclose(temp_file);

    rename(temp_path, file_path); // Atomic rename
}

// Remove a file
void io_data_store_remove_file(io_data_store_t *h, const char *filename) {
    char file_path[512];
    generate_file_path(file_path, sizeof(file_path), h->base_path, filename);
    unlink(file_path);
}
