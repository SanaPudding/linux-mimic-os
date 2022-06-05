#ifndef FS_H
#define FS_H
#include "../common.h"
#include "../lib.h"
#include "../multiboot.h"

#define FS_BLOCK_SIZE_BYTES 4096
#define MAX_FILENAME_LENGTH 32
#define FS_TYPE_RTC 0
#define FS_TYPE_DIR 1
#define FS_TYPE_FILE 2
#define MAX_NUM_FILE 63

#ifndef ASM

typedef struct fs_data_blk_t {
    uint8_t data[FS_BLOCK_SIZE_BYTES];
} fs_data_blk_t;
STATIC_ASSERT(sizeof(fs_data_blk_t) == FS_BLOCK_SIZE_BYTES);

typedef struct fs_inode_blk_t {
    uint32_t len_in_bytes;
    uint32_t data_block_ids[FS_BLOCK_SIZE_BYTES / sizeof(uint32_t) - 1]; 
} __attribute__((packed)) fs_inode_blk_t;
STATIC_ASSERT(sizeof(fs_inode_blk_t) == FS_BLOCK_SIZE_BYTES)

typedef struct fs_boot_blk_dentry_t {
    char filename[32];
    uint32_t filetype;
    uint32_t inode_idx;
    uint8_t reserved[24];
} __attribute__((packed)) fs_boot_blk_dentry_t; 
STATIC_ASSERT(sizeof(fs_boot_blk_dentry_t) == 64);

#define NUM_DENTRIES ((FS_BLOCK_SIZE_BYTES / sizeof(fs_boot_blk_dentry_t)) - 1)

typedef struct fs_boot_blk_t {
    uint32_t dentry_count;
    uint32_t inode_count;
    uint32_t data_blk_count;
    uint8_t reserved[52];
    // https://gcc.gnu.org/onlinedocs/gcc/Zero-Length.html
    //      "A zero-length array can be useful as the last element of a structure that is really a 
    //       header for a variable-length object...The preferred mechanism to declare variable-length types...
    //       is the ISO C99 flexible array member"
    // Be sure to validate against dentry_count to prevent against reading garbage data, e.g. dentries[i] works for i < dentry_count !!!!
    fs_boot_blk_dentry_t dentries[];
} __attribute__((packed)) fs_boot_blk_t;

extern fs_boot_blk_t *fs_boot_blk_location;

// Each of the below return -1 on failure - that is, when a file doesn't exist or an index is invalid
// Bottom two functions popoulate the dentry passed in.
int32_t read_dentry_by_name(const char* fname, fs_boot_blk_dentry_t* dentry);
int32_t read_dentry_by_index(uint32_t index, fs_boot_blk_dentry_t* dentry);
// Returns -1 if an invalid inode was given.
int32_t read_data(uint32_t inode, uint32_t offset, uint8_t* buf, uint32_t length);
int32_t dentry_strcmp(const char* term_str, const char* dentry_str);

fs_data_blk_t* ith_data_blk(uint32_t i);
fs_inode_blk_t* ith_inode_blk(uint32_t i);

int fs_init(module_t fs_mod);

#endif
#endif
