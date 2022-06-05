#include "../process/file.h"
#include "fs_interface.h"
#include "memfs.h"

/* operation structs for file system */
file_operations_t file_system_directory_ops = {
    .open = fs_open,
    .close = fs_close,
    .read = fs_dir_read,
    .write = fs_write
};

file_operations_t file_system_file_ops = {
    .open = fs_open,
    .close = fs_close,
    .read = fs_file_read,
    .write = fs_write
};

/* dummy function, see generic_open for real functionality */
int32_t fs_open(void) {return 0;}

/* dummy function, see generic_close for real functionality */
int32_t fs_close(void) {return 0;}

/*
 * fs_dir_read
 *     DESCRIPTION: Read a file name indexed by the offset field in the
 *                  fc file_context struct, store nbytes length of the
 *                  file name into the input buf.
 *     INPUTS: fc -- file context that holds info about a directory.
 *            buf -- the buffer receiving all the bytes read.
 *         nbytes -- number of bytes to be read.
 *     RETURN VALUE: number of bytes read upon success, -1 upon failure.
 */
int32_t fs_dir_read(file_context* fc, uint8_t* buf, int32_t nbytes) {
    if (!fc || !buf) {return -1;}   // null check
    if (!nbytes) {return 0;}        // read 0 byte

    // the file descriptor is of "directory" type
    if (fc->filetype == FILETYPE_DIR) {
        // We must be reading a directory
        // The offset for the directory represents the nth filename that we read
        // If we're given a buffer that's too small, we fill what we can and increment the offset
        // (it's the user's fault if they can't be bothered to give a buffer that's long enough)
        int32_t counter = 0;
        uint32_t dentry_id = fc->offset;
        if (dentry_id >= fs_boot_blk_location->dentry_count) {return 0;}
        // Copy as much of the filename as we can
        while (counter < MAX_FILENAME_LENGTH && counter < nbytes) {
            buf[counter] = fs_boot_blk_location->dentries[dentry_id].filename[counter];
            counter++;
        }
        // Move to the next filename
        fc->offset += 1;
        return counter;
    // the file descriptor is not of "directory" type
    } else {return -1;}
}

/*
 * fs_file_read
 *     DESCRIPTION: Read in nbytes of data stored in the file specified by
 *                  the fc file context, and store those data into the input buf.
 *     INPUTS: fc -- file context that holds info about a file.
 *            buf -- the buffer receiving all the bytes read.
 *         nbytes -- number of bytes to be read.
 *     RETURN VALUE: number of bytes read upon success, -1 upon failure.
 */
int32_t fs_file_read(file_context* fc, uint8_t* buf, int32_t nbytes) {
    if (!fc || !buf) {return -1;}   // null check
    if (!nbytes) {return 0;}        // read 0 byte

    // the file descriptor is of "file" type
    if (fc->filetype == FILETYPE_FILE) {
        int32_t ret_val = read_data(fc->inode, fc->offset, buf, nbytes);
        if (ret_val == -1) {return -1;}
        else {fc->offset += ret_val;}
        return ret_val;
    // the file descriptor is not of "file" type
    } else {return -1;}
}

/* dummy function, no real functionality */
int32_t fs_write(file_context* fc, const uint8_t* buf, int32_t nbytes) {
    return -1;
}
