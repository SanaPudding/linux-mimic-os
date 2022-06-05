#ifndef FILE_H
#define FILE_H

#define FAIL_FD     -1
#define MAX_NUM_FD  8

#define FILETYPE_DEV    0
#define FILETYPE_DIR    1
#define FILETYPE_FILE   2
#define FILETYPE_UNKOWN 0xFFFFFFFF
#define STDIN_FD 0
#define STDOUT_FD 1

#ifndef ASM

#include "../types.h"

typedef struct {
    uint32_t filetype;
    uint32_t inode;
    uint32_t offset;
} file_context;

/* This structure serves as a generic interface for different
 * open/close/read/write functions */
typedef struct {
    int32_t (*open)  (void);
    int32_t (*close) (void);
    int32_t (*read)  (file_context*, uint8_t*, int32_t);
    int32_t (*write) (file_context*, const uint8_t*, int32_t);
} file_operations_t;

/* file descriptor struct for file, directory, and rtc */
typedef struct {
    file_operations_t* operations;
    file_context context;
    uint32_t present;
} file_descriptor_t;

int32_t generic_open  (const uint8_t* filename);
int32_t generic_close (int32_t fd);
int32_t generic_read  (int32_t fd, uint8_t* buf, int32_t nbytes);
int32_t generic_write (int32_t fd, const uint8_t* buf, int32_t nbytes);

int32_t fd_close_noop(void);
int32_t fd_open_noop (void);
int32_t fd_read_noop (file_context*, uint8_t*, int32_t);
int32_t fd_write_noop(file_context*, const uint8_t*, int32_t);

void initialize_fd_array(file_descriptor_t* fd_array);
void close_fd(file_descriptor_t* fdt);

#endif /* ASM */
#endif
