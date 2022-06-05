#include "memfs.h"
#include "../multiboot.h"
#include "../paging.h"

fs_boot_blk_t* fs_boot_blk_location;

// Input: Index of the data block i
// Output: Address of the ith data block as a struct pointer
// Side effects: None
// Preconditions: i is in range
fs_data_blk_t* ith_data_blk(uint32_t i) {
    return (fs_data_blk_t*)(fs_boot_blk_location) 
                + (fs_boot_blk_location->inode_count + 1) // Start at N+1
                + i;
}


// Input: Index of inode block i
// Output: Address of the ith inode block as a struct pointer
// Side effects: None
// Preconditions: i is in range
fs_inode_blk_t* ith_inode_blk(uint32_t i) {
    return (fs_inode_blk_t*)(fs_boot_blk_location) + (1 + i); // Start at 1
}


// Input: Module to the filesystem given by GRUB
// Output: 0
// Side effects: Modifies fs_boot_blk_location and initializes the fake file descriptors
int fs_init(module_t fs_mod) {
    fs_boot_blk_location = (fs_boot_blk_t*)(fs_mod.mod_start);
    return 0;
}

// Reads a dentry by a string.
// Input: Filename as a char pointer, dentry to populate
// Output: -1 if fname or dentry is null, or if the filename doesn't exist. 0 if successful
// Side effects: If successful, populates dentry
int32_t read_dentry_by_name(const char* fname, fs_boot_blk_dentry_t* dentry) {
    if (!dentry || !fname) return -1;
    else {
        // Just iterate over the dentries and compare strings until we find a match.
        uint32_t i;
        for (i = 0; i < fs_boot_blk_location->dentry_count; i++) {
            int32_t comp_res = dentry_strcmp((const char*)fname, fs_boot_blk_location->dentries[i].filename);
            if (comp_res == 0) {
                *dentry = fs_boot_blk_location->dentries[i];
                return 0;
            }
        }
    }
    return -1;
}

// Reads a dentry by its dentry index
// Input: Directory entry as an index, dentry pointer to populate
// Output: -1 if the index is out of range or if dentry is null, 0 else.
// Side effects: Populates *dentry with value of the filesystem dentry
int32_t read_dentry_by_index(uint32_t index, fs_boot_blk_dentry_t* dentry) {
    if (!dentry || index >= fs_boot_blk_location->dentry_count) return -1;
    else {
        *dentry = fs_boot_blk_location->dentries[index];
        return 0;
    }
}

// Reads [length] bytes into a buffer of inode [inode], starting at offset [offset]
// Inputs:
//      inode: Inode index
//      offset: Byte offset to start reading data
//      buf: Buffer to fill
//      length: Length of buffer to fill
// Outputs: -1 if inode is out of range, or if buf is null. Number of bytes read otherwise.
// Side effects: Fills [length] bytes into [buf].
int32_t read_data(uint32_t inode, uint32_t offset, uint8_t* buf, uint32_t length) {
    // Validate inode number.
    if (inode >= fs_boot_blk_location->inode_count || !buf) return -1;
    // Postcondition: valid inode number
    fs_inode_blk_t* this_inode = ith_inode_blk(inode);
    
    uint32_t i;
    // Block ID validation 
    for (i = 0; i < CEILDIV(this_inode->len_in_bytes, FS_BLOCK_SIZE_BYTES); i++) {
        if (this_inode->data_block_ids[i] >= fs_boot_blk_location->data_blk_count) {
            return -1;
        }
    }

    uint32_t bytes_read = 0;
    while (bytes_read < length) {
        // We first determine the ith datablock that we're at,
        // find *which* datablock is the ith datablock, and then
        // we index into that datablock.
        uint32_t num_datablock = offset / FS_BLOCK_SIZE_BYTES; // The datablock we're at
        uint32_t datablock_inner_offset = offset % FS_BLOCK_SIZE_BYTES; // Index within datablock
        if (offset >= this_inode->len_in_bytes) break;
        // Access the data block pointed by the number datablock we're at
        buf[bytes_read] = ith_data_blk(this_inode->data_block_ids[num_datablock])->data[datablock_inner_offset];
        bytes_read++;
        offset++;
    }
    return bytes_read;
}

// Compares a null-terminated string to a dentry string.
// Inputs:
//      term_str: a null terminated string
//      dentry_str: a potentially not null terminated string of max length 32
// Outputs: 0 if matching, -1 if not matching.
// Side effects: none
int32_t dentry_strcmp(const char* term_str, const char* dentry_str) {
    if ((!term_str && dentry_str) || (!dentry_str && term_str)) return -1; // Null won't equal not null.
    if (term_str == dentry_str && !term_str) return 0; // Null equals null.

    uint32_t comps = 0; 

    for (comps = 0; comps < MAX_FILENAME_LENGTH; comps++) {
        if (term_str[comps] != dentry_str[comps]) return -1;
        if (!term_str[comps]) break;
    }
    // Postcondition: term_str[comps] == dentry_str[comps]

    // Handle case: we ran out of dentry characters. If the 33rd char of term_str is \0, then they are equal; otherwise, they aren't.
    if (comps == MAX_FILENAME_LENGTH && term_str[MAX_FILENAME_LENGTH]) return -1;
    else return 0; // They must've been equal; if they weren't, we would've returned -1 by now.
}
