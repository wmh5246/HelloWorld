#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "cache.h"
#include "jbod.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int num_queries = 0;
static int num_hits = 0;

int cache_create(int num_entries) {
    // check if the cache has already been created
    if (cache != NULL) {
        debug_log("Cache already created\n");
        return -1;
    }
    // check if the cache size is valid
    if (num_entries < 2 || num_entries > 4096) {
        debug_log("Invalid cache size\n");
        return -1;
    }

    cache_size = num_entries;

    // allocate the cache
    cache = (cache_entry_t *)malloc(num_entries * sizeof(cache_entry_t));
    if (cache == NULL) {
        debug_log("Failed to allocate cache\n");
        return -1;
    }

    // initialize the cache
    for (int i = 0; i < cache_size; i++) {
        cache[i].valid = 0;
        cache[i].disk_num = -1;
        cache[i].block_num = -1;
        cache[i].num_accesses = 0;
    }

    return 1;
}

int cache_destroy(void) {
    // check if the cache has already been destroyed
    if (cache == NULL) {
        debug_log("Cache not created\n");
        return -1;
    }

    // free the cache
    free(cache);
    cache = NULL;
    cache_size = 0;

    return 1;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
    // check if the cache has been created
    if (cache == NULL) {
        debug_log("Cache not created\n");
        return -1;
    }
    // check if the buffer is valid
    if (buf == NULL) {
        debug_log("Invalid buffer\n");
        return -1;
    }

    num_queries++;
    // check if the block is in the cache
    for (int i = 0; i < cache_size; i++) {
        if (cache[i].valid == 1 && cache[i].disk_num == disk_num && cache[i].block_num == block_num) {
            memcpy(buf, cache[i].block, JBOD_BLOCK_SIZE);
            num_hits++;
            cache[i].num_accesses++;
            return 1;
        }
    }

    return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
    // check if the cache has been created
    if (cache == NULL) {
        debug_log("Cache not created\n");
        return;
    }
    // check if the buffer is valid
    if (buf == NULL) {
        debug_log("Invalid buffer\n");
        return;
    }

    // update the block in the cache
    for (int i = 0; i < cache_size; i++) {
        if (cache[i].valid && cache[i].disk_num == disk_num && cache[i].block_num == block_num) {
            memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);
            cache[i].num_accesses = 1;
            return;
        }
    }
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
    // check if the cache has been created
    if (cache == NULL) {
        debug_log("Cache not created\n");
        return -1;
    }
    // check if the buffer is valid
    if (buf == NULL) {
        debug_log("Invalid buffer\n");
        return -1;
    }
    // check if the disk number and block number are valid
    if (disk_num < 0 || disk_num >= JBOD_NUM_DISKS) {
        debug_log("Invalid disk number\n");
        return -1;
    }
    // check if the block number is valid
    if (block_num < 0 || block_num >= JBOD_NUM_BLOCKS_PER_DISK) {
        debug_log("Invalid block number\n");
        return -1;
    }

    // check if the block is already in the cache
    for (int i = 0; i < cache_size; i++) {
        if (cache[i].valid && cache[i].disk_num == disk_num && cache[i].block_num == block_num) {
            return -1;
        }
    }

    // find an empty slot
    for (int i = 0; i < cache_size; i++) {
        if (!cache[i].valid) {
            cache[i].valid = 1;
            cache[i].disk_num = disk_num;
            cache[i].block_num = block_num;
            memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);
            cache[i].num_accesses = 1;
            // printf("cache_insert %d %d\n", disk_num, block_num);
            return 1;
        }
    }

    // no empty slot, so get the least recently used entry
    int min_accesses = cache[0].num_accesses;
    int min_index = 0;
    for (int i = 1; i < cache_size; i++) {
        if (cache[i].num_accesses < min_accesses) {
            min_accesses = cache[i].num_accesses;
            min_index = i;
        }
    }
    // insert the block into the entry
    cache[min_index].valid = 1;
    cache[min_index].disk_num = disk_num;
    cache[min_index].block_num = block_num;
    memcpy(cache[min_index].block, buf, JBOD_BLOCK_SIZE);
    cache[min_index].num_accesses = 1;

    return 1;
}

bool cache_enabled(void) {
	return cache != NULL && cache_size > 0;
}

void cache_print_hit_rate(void) {
	fprintf(stderr, "num_hits: %d, num_queries: %d\n", num_hits, num_queries);
	fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}
