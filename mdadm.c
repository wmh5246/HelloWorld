#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cache.h"
#include "jbod.h"
#include "mdadm.h"

// Mounts the JBOD device
int mdadm_mount(void) {
    // Your implementation for mounting JBOD
    return -1;
}

// Unmounts the JBOD device
int mdadm_unmount(void) {
    // Your implementation for unmounting JBOD
    return -1;
}

// Grants write permission to the JBOD device
int mdadm_write_permission(void) {
    // Your implementation for granting write permission
    return -1;
}

// Revokes write permission from the JBOD device
int mdadm_revoke_write_permission(void) {
    // Your implementation for revoking write permission
    return -1;
}

// Reads data from JBOD, using the cache if possible
int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
    if (buf == NULL || len == 0 || len > 1024 || (addr + len) > 1048576) {
        return -1;
    }

    uint32_t current_addr = addr;
    uint32_t remaining_len = len;
    uint8_t temp_buf[JBOD_BLOCK_SIZE];

    while (remaining_len > 0) {
        uint32_t disk_num = current_addr / JBOD_DISK_SIZE;
        uint32_t block_num = (current_addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
        uint32_t block_offset = current_addr % JBOD_BLOCK_SIZE;
        uint32_t bytes_to_read = JBOD_BLOCK_SIZE - block_offset;
        if (bytes_to_read > remaining_len) {
            bytes_to_read = remaining_len;
        }

        // Use cache if enabled and block is present
        if (cache_enabled() && cache_lookup(disk_num, block_num, temp_buf) == 1) {
            memcpy(buf, temp_buf + block_offset, bytes_to_read);
        } else {
            // Read block from JBOD if not in cache
            if (jbod_client_operation(JBOD_READ_BLOCK, disk_num, block_num, temp_buf) != 0) {
                return -1;
            }
            // Insert block into cache if enabled
            if (cache_enabled()) {
                cache_insert(disk_num, block_num, temp_buf);
            }
            memcpy(buf, temp_buf + block_offset, bytes_to_read);
        }

        current_addr += bytes_to_read;
        buf += bytes_to_read;
        remaining_len -= bytes_to_read;
    }

    return len;
}

// Writes data to JBOD, updating the cache if enabled
int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
    if (buf == NULL || len == 0 || len > 1024 || (addr + len) > 1048576) {
        return -1;
    }

    uint32_t current_addr = addr;
    uint32_t remaining_len = len;
    uint8_t temp_buf[JBOD_BLOCK_SIZE];

    while (remaining_len > 0) {
        uint32_t disk_num = current_addr / JBOD_DISK_SIZE;
        uint32_t block_num = (current_addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
        uint32_t block_offset = current_addr % JBOD_BLOCK_SIZE;
        uint32_t bytes_to_write = JBOD_BLOCK_SIZE - block_offset;
        if (bytes_to_write > remaining_len) {
            bytes_to_write = remaining_len;
        }

        // Read block from JBOD before writing to it
        if (jbod_client_operation(JBOD_READ_BLOCK, disk_num, block_num, temp_buf) != 0) {
            return -1;
        }
        // Modify the block with the new data
        memcpy(temp_buf + block_offset, buf, bytes_to_write);

        // Write the modified block back to JBOD
        if (jbod_client_operation(JBOD_WRITE_BLOCK, disk_num, block_num, temp_buf) != 0) {
            return -1;
        }
        // Update the cache with the new block if enabled
        if (cache_enabled()) {
            cache_update(disk_num, block_num, temp_buf);
        }

        current_addr += bytes_to_write;
        buf += bytes_to_write;
        remaining_len -= bytes_to_write;
    }

    return len;
}
