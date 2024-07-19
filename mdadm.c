// CMPSC 311 SU24
// LAB 2
// Wentao He
// wmh5246@psu.edu

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "jbod.h"
#include "util.h"

static int mounted = 0;
static int writable = 0;

// create op code for jbod_operation
static uint32_t create_op_code(uint32_t cmd, uint32_t disk_id, uint32_t block_id) {
    uint32_t op_code = 0;
    op_code |= (cmd & 0x3F);
    op_code |= (disk_id & 0xF) << 6;
    op_code |= (block_id & 0xFF) << 10;
    return op_code;
}

int mdadm_mount(void) {
    // check if already mounted
    if (mounted) {
        debug_log("Mount failed: already mounted\n");
        return -1;
    }
    // mound the jbod
    uint32_t op = create_op_code(JBOD_MOUNT, 0, 0);
    int status = jbod_operation(op, NULL);
    // check if mount successful
    if (status == 0) {
        mounted = 1;
        debug_log("Mount successful\n");
        return 1;
    }
    else {
        debug_log("Mount failed\n");
        return -1;
    }
}

int mdadm_unmount(void)
{
    // check if already unmounted
    if (!mounted) {
        debug_log("Unmount failed: not mounted\n");
        return -1;
    }
    // unmount the jbod
    uint32_t op = create_op_code(JBOD_UNMOUNT, 0, 0);
    int status = jbod_operation(op, NULL);
    // check if unmount successful
    if (status == 0) {
        mounted = 0;
        debug_log("Unmount successful\n");
        return 1;
    } else {
        debug_log("Unmount failed\n");
        return -1;
    }
}

int mdadm_read(uint32_t start_addr, uint32_t read_len, uint8_t *read_buf)
{
    // check if mounted
    if (!mounted) {
        debug_log("Read failed: not mounted\n");
        return -1;
    }
    if (read_len == 0 && read_buf == NULL) {
        debug_log("Read successful: 0-length read\n");
        return 0;
    }
    // check if the parameters are valid
    if ((start_addr + read_len) > JBOD_DISK_SIZE * JBOD_NUM_DISKS || read_len > 1024 || read_buf == NULL) {
        debug_log("Read failed: invalid parameters\n");
        return -1;
    }


    uint32_t total_block_count = start_addr / JBOD_BLOCK_SIZE;
    uint32_t offset = start_addr % JBOD_BLOCK_SIZE;
    uint32_t cur_disk = total_block_count / JBOD_NUM_BLOCKS_PER_DISK;
    uint32_t cur_block = total_block_count % JBOD_NUM_BLOCKS_PER_DISK;
    uint32_t bytes_left = read_len;

	// seek to the cur disk
    uint32_t op = create_op_code(JBOD_SEEK_TO_DISK, cur_disk, 0);
    int status = jbod_operation(op, NULL);
    if (status != 0) {
        debug_log("Read failed: seek error\n");
        return -1;
    }

	// seek to the cur block
    op = create_op_code(JBOD_SEEK_TO_BLOCK, 0, cur_block);
    status = jbod_operation(op, NULL);
    if (status != 0) {
        debug_log("Read failed: seek error\n");
        return -1;
    }

    while (bytes_left > 0) {
		// if the current disk is full, move to the next disk
        if (cur_block == JBOD_NUM_BLOCKS_PER_DISK) {
            cur_disk++; cur_block = 0; // move to the next disk, block reset to 0
            op = create_op_code(JBOD_SEEK_TO_DISK, cur_disk, 0);
            status = jbod_operation(op, NULL);
            if (status != 0) {
                debug_log("Read failed: seek error\n");
                return -1;
            }
            op = create_op_code(JBOD_SEEK_TO_BLOCK, 0, cur_block);
            status = jbod_operation(op, NULL);
            if (status != 0) {
                debug_log("Read failed: seek error\n");
                return -1;
            }
        }

        // calculate the bytes to read
        uint32_t bytes_to_read = (JBOD_BLOCK_SIZE - offset > bytes_left) ? 
                                    bytes_left : JBOD_BLOCK_SIZE - offset;
		// read the block
        char temp_buf[JBOD_BLOCK_SIZE];
        if (cache_enabled() && cache_lookup(cur_disk, cur_block, (uint8_t*)temp_buf) == 1) {
            debug_log("Cache hit\n");
        } else {
            op = create_op_code(JBOD_READ_BLOCK, 0, 0);
            int status = jbod_operation(op, (uint8_t*)temp_buf);
            if (status != 0) {
                debug_log("Read failed: read error\n");
                return -1;
            }
            if (cache_enabled()) {
                cache_insert(cur_disk, cur_block, (uint8_t*)temp_buf);
            }
        }
        
		// copy the content to read_buf
        for (int i = 0; i < bytes_to_read; i++) {
            read_buf[read_len - bytes_left + i] = temp_buf[offset + i];
        }
        // move to the next block
        cur_block++;
        bytes_left -= bytes_to_read;
        offset = (offset + bytes_to_read) % JBOD_BLOCK_SIZE;
    }

    debug_log("Read successful\n");
    return read_len;
}

int mdadm_write_permission(void){
    writable = 1;
    int status = jbod_operation(JBOD_WRITE_PERMISSION, NULL);
    return status;
}


int mdadm_revoke_write_permission(void){
    writable = 0;
    int status = jbod_operation(JBOD_REVOKE_WRITE_PERMISSION, NULL);
	return status;
}


int mdadm_write(uint32_t start_addr, uint32_t write_len, const uint8_t *write_buf) {
    // check if mounted and writable
	if (!mounted || !writable) {
        return -1;
    }

    if (write_len == 0 && write_buf == NULL) {
        return 0;
    }
    // check if the parameters are valid
    if ((start_addr + write_len) > JBOD_DISK_SIZE * JBOD_NUM_DISKS || write_len > 1024 || write_buf == NULL) {
        return -1;
    }

    uint32_t total_block_count = start_addr / JBOD_BLOCK_SIZE;
    uint32_t offset = start_addr % JBOD_BLOCK_SIZE;
    uint32_t cur_disk = total_block_count / JBOD_NUM_BLOCKS_PER_DISK;
    uint32_t cur_block = total_block_count % JBOD_NUM_BLOCKS_PER_DISK;
    uint32_t bytes_left = write_len;

    // seek to the cur disk
    uint32_t op = create_op_code(JBOD_SEEK_TO_DISK, cur_disk, 0);
    int status = jbod_operation(op, NULL);
    if (status != 0) {
        debug_log("Read failed: seek error\n");
        return -1;
    }
    // seek to the cur block
    op = create_op_code(JBOD_SEEK_TO_BLOCK, 0, cur_block);
    status = jbod_operation(op, NULL);
    if (status != 0) {
        debug_log("Read failed: seek error\n");
        return -1;
    }

    while (bytes_left > 0) {
        // if the current disk is full, move to the next disk
        if (cur_block == JBOD_NUM_BLOCKS_PER_DISK) {
            cur_disk++; cur_block = 0;
            op = create_op_code(JBOD_SEEK_TO_DISK, cur_disk, 0);
            status = jbod_operation(op, NULL);
            if (status != 0) {
                debug_log("Read failed: seek error\n");
                return -1;
            }
            op = create_op_code(JBOD_SEEK_TO_BLOCK, 0, cur_block);
            status = jbod_operation(op, NULL);
            if (status != 0) {
                debug_log("Read failed: seek error\n");
                return -1;
            }
        }
        // calculate the bytes to write
        uint32_t bytes_to_write = (JBOD_BLOCK_SIZE - offset > bytes_left) ? 
                                    bytes_left : JBOD_BLOCK_SIZE - offset;

        // read the block to be written
        char temp_buf[JBOD_BLOCK_SIZE];

        op = create_op_code(JBOD_READ_BLOCK, 0, 0);
        int status = jbod_operation(op, (uint8_t*)temp_buf);
        if (status != 0) {
            debug_log("Read failed: read error\n");
            return -1;
        }
        if (cache_enabled()) {
            int res = cache_insert(cur_disk, cur_block, (uint8_t*)temp_buf);
            // update the cache if the block is already in the cache
            if (res == -1) cache_update(cur_disk, cur_block, (uint8_t*)temp_buf); 
        }
        // write the target content to temp_buf
        for (int i = 0; i < bytes_to_write; i++) {
            temp_buf[offset + i] = write_buf[write_len - bytes_left + i];
        }

        // seek to the target block (block_no increment 1 after each write)
        op = create_op_code(JBOD_SEEK_TO_BLOCK, 0, cur_block);
        status = jbod_operation(op, NULL);
        if (status != 0) {
            debug_log("Write failed: seek error\n");
            return -1;
        }
        // write the block
        op = create_op_code(JBOD_WRITE_BLOCK, 0, 0);
        status = jbod_operation(op, (uint8_t*)temp_buf);
        if (status != 0) {
            debug_log("Read failed: read error\n");
            return -1;
        }
        // move to the next block
        cur_block++;
        bytes_left -= bytes_to_write;
        offset = (offset + bytes_to_write) % JBOD_BLOCK_SIZE;
    }
    return write_len;
}

