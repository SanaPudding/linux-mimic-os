#ifndef FS_INTERFACE_H
#define FS_INTERFACE_H

#include "../process/file.h"
#include "memfs.h"

/* file system operation struct */
extern file_operations_t file_system_directory_ops;
extern file_operations_t file_system_file_ops;

int32_t fs_open  (void);
int32_t fs_close (void);
int32_t fs_dir_read  (file_context* fc, uint8_t* buf, int32_t nbytes);
int32_t fs_file_read (file_context* fc, uint8_t* buf, int32_t nbytes);
int32_t fs_write (file_context* fc, const uint8_t* buf, int32_t nbytes);

#endif
