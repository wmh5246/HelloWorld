#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "cache.h"
#include "jbod.h"

static cache_entry_t *cache = NULL; // Pointer to cache array
static int cache_size = 0; // Size of the cache
static int num_queries = 0; // Number of cache queries
static int num_hits = 0; // Number of cache hits

// Creates the cache with a specified number of entries
int cache_create(int num_entries) {
    // Validate number of entries and ensure cache is not already created
    if (num_entries < 2 || num_entries > 4096 || cache != NULL) {
        return -1;
    }
    // Allocate memory for the cache
    cache = (cache_entry_t *)malloc(num_entries * sizeof(cache_entry_t));
    if (cache == NULL) {
        return -1;
    }
    cache_size = num_entries;
    // Initialize all cache entries as invalid
    for (int i = 0; i < cache_size; i++) {
        cache[i].valid = false;
    }
    return 1;
}

// Destroys the cache and frees allocated memory
int cache_destroy(void) {
    // Ensure cache is already created
    if (cache == NULL) {
        return -1;
    }
    // Free cache memory and reset variables
    free(cache);
    cache = NULL;
    cache_size = 0;
    return 1;
}

// Looks up a block in the cache
int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
    if (cache == NULL || buf == NULL) {
        return -1;
    }
    num_queries++;
    // Search for the block in the cache
    for (int i = 0; i < cache_size; i++) {
        if (cache[i].valid && cache[i].disk_num == disk_num && cache[i].block_num == block_num) {
            // Block found, copy it to the buffer and increment access count
            memcpy(buf, cache[i].block, JBOD_BLOCK_SIZE);
            cache[i].num_accesses++;
            num_hits++;
            return 1;
        }
    }
    // Block not found in cache
    return -1;
}

// Inserts a new block into the cache
int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
    if (cache == NULL || buf == NULL) {
        return -1;
    }
    // Ensure the block is not already in the cache
    for (int i = 0; i < cache_size; i++) {
        if (cache[i].valid && cache[i].disk_num == disk_num && cache[i].block_num == block_num) {
            return -1;
        }
    }
    // Find an empty or least frequently used (LFU) cache entry
    int lfu_index = -1;
    int min_accesses = INT_MAX;
    for (int i = 0; i < cache_size; i++) {
        if (!cache[i].valid) {
            lfu_index = i;
            break;
        }
        if (cache[i].num_accesses < min_accesses) {
            min_accesses = cache[i].num_accesses;
            lfu_index = i;
        }
    }
    // Insert the new block into the selected cache entry
    if (lfu_index != -1) {
        cache[lfu_index].valid = true;
        cache[lfu_index].disk_num = disk_num;
        cache[lfu_index].block_num = block_num;
        memcpy(cache[lfu_index].block, buf, JBOD_BLOCK_SIZE);
        cache[lfu_index].num_accesses = 1;
        return 1;
    }
    return -1;
}

// Updates an existing block in the cache
void cache_update(int disk_num, int block_num, const uint8_t *buf) {
    if (cache == NULL || buf == NULL) {
        return;
    }
    // Search for the block in the cache and update it if found
    for (int i = 0; i < cache_size; i++) {
        if (cache[i].valid && cache[i].disk_num == disk_num && cache[i].block_num == block_num) {
            memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);
            cache[i].num_accesses++;
            return;
        }
    }
}

// Checks if the cache is enabled
bool cache_enabled(void) {
    return cache != NULL && cache_size > 0;
}

// Prints the cache hit rate statistics
void cache_print_hit_rate(void) {
    fprintf(stderr, "num_hits: %d, num_queries: %d\n", num_hits, num_queries);
    if (num_queries > 0) {
        fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
    } else {
        fprintf(stderr, "Hit rate: -nan%%\n");
    }
}
