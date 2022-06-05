#include "file.h"
#include "process.h"
#include "../lib.h"
#include "../device-drivers/rtc.h"
#include "../device-drivers/terminal.h"
#include "../memfs/memfs.h"
#include "../memfs/fs_interface.h"

// Description: Filler function to put into file_operations_t for an unsupported operation (close)
// Inputs/Outputs: None
int32_t fd_close_noop(void) { return -1; }

// Description: Filler function to put into file_operations_t for an unsupported operation (open)
// Inputs/Outputs: None
int32_t fd_open_noop(void) { return -1; }

// Description: Filler function to put into file_operations_t for an unsupported operation (read)
// Inputs: Interface for file_operations_t struct
// Outputs: -1
int32_t fd_read_noop(file_context* _a, uint8_t* _b , int32_t _c) {
    (void)_a;
    (void)_b;
    (void)_c;
    return -1;
}

// Description: Filler function to put into file_operations_t for an unsupported operation (write)
// Inputs: Interface for file_operations_t struct
// Outputs: -1
int32_t fd_write_noop(file_context* _a, const uint8_t* _b, int32_t _c) {
    (void)_a;
    (void)_b;
    (void)_c;
    return -1;
}


/* file scope functions */
static int32_t get_allocatable_fd(pcb_t* pcb);

/*
 * generic_open
 *     DESCRIPTION: A generic function interface for the open system call operation.
 *     INPUTS: filename -- name of the file/directory/device to be opened.
 *     RETURN VALUE: allocated fd (an integer index), or -1 upon failure.
 */
int32_t generic_open(const uint8_t* filename) {
    // sanity checks
    if (!filename) {return -1;}
    
    pcb_t* curr_pcb = get_current_pcb();
    if (!curr_pcb) {return -1;}
    uint8_t* k_filename = (uint8_t*) translate_user_to_kernel(filename, curr_pcb->pid);
    if (!k_filename) return -1;

    if (strlen((const char*) k_filename) == 0) {return -1;}

    int32_t new_fd = get_allocatable_fd(curr_pcb);
    if (new_fd == FAIL_FD) {return -1;}
    
    file_descriptor_t* new_fdt = &(curr_pcb->fd_array[new_fd]);

    // open operation for file or directory
    fs_boot_blk_dentry_t temp_dentry;
    if (read_dentry_by_name((const char*) k_filename, &temp_dentry) == -1) {return -1;}
    else {
        // asssign different fops based on file type
        if (temp_dentry.filetype == FILETYPE_DIR) {
            new_fdt->operations = &file_system_directory_ops;
        } else if (temp_dentry.filetype == FILETYPE_FILE) {
            new_fdt->operations = &file_system_file_ops;
        } else if (temp_dentry.filetype == FILETYPE_DEV) {
            new_fdt->operations = &rtc_ops;
        } else {return -1;}
        new_fdt->context.filetype = temp_dentry.filetype;
        new_fdt->context.inode = temp_dentry.inode_idx;
        new_fdt->context.offset = 0;
        new_fdt->present = 1;
    }

    
    if (new_fdt->operations->open == NULL) {return -1;}             // the fdt's open operation doesn't exist
    if ((*new_fdt->operations->open)() == -1) {return -1;}          // the open operation failed

    // open operation succeeded, return allocated fd number
    return new_fd;
}

/*
 * generic_close
 *     DESCRIPTION: A generic function interface for the close system call operation.
 *     INPUTS: fd -- index to the file descriptor be be closed.
 *     RETURN VALUE: 0 upon success, -1 upon failure.
 */
int32_t generic_close(int32_t fd) {
    // sanity checks
    if (fd < 2 || fd >= MAX_NUM_FD) {return -1;}                // default fds 0 and 1 shouldn't be closed

    pcb_t* curr_pcb = get_current_pcb();
    if (!curr_pcb) {return -1;}

    if (curr_pcb->fd_array[fd].present == 0 ||                  // the fdt is not present
        curr_pcb->fd_array[fd].operations == NULL ||            // the fdt's operation struct doesn't exist
        curr_pcb->fd_array[fd].operations->close == NULL ||     // the fdt's close operation doesn't exist
        (*curr_pcb->fd_array[fd].operations->close)() == -1     // the close operation failed
        ) {return -1;}

    // "close"/clear the fdt
    close_fd(&(curr_pcb->fd_array[fd]));
    return 0;
}

/*
 * generic_read
 *     DESCRIPTION: A generic function interface for the read system call operation.
 *     INPUTS: fd -- index to the file descriptor be be read.
 *            buf -- buffer receiving all the bytes read.
 *         nbytes -- number of bytes to be read.
 *     RETURN VALUE: number of bytes read.
 */
int32_t generic_read(int32_t fd, uint8_t* k_buf, int32_t nbytes) {
    // sanity checks
    if (fd < 0 || fd >= MAX_NUM_FD) {return -1;}

    pcb_t* curr_pcb = get_current_pcb();
    if (!curr_pcb) {return -1;}
    uint8_t* buf = (uint8_t*)translate_user_to_kernel(k_buf, curr_pcb->pid);
    if (!buf) return -1;

    if (curr_pcb->fd_array[fd].present == 0 ||                  // the fdt is not present
        curr_pcb->fd_array[fd].operations == NULL ||            // the fdt's operation struct doesn't exist
        curr_pcb->fd_array[fd].operations->read == NULL         // the fdt's read operation doesn't exist
        ) {return -1;}

    return (*curr_pcb->fd_array[fd].operations->read)(&(curr_pcb->fd_array[fd].context), buf, nbytes);
}

/*
 * generic_write
 *     DESCRIPTION: A generic function interface for the write system call operation.
 *     INPUTS: fd -- index to the file descriptor be be written.
 *            buf -- buffer holding all the bytes to be written.
 *         nbytes -- number of bytes to be written.
 *     RETURN VALUE: number of bytes written.
 */
int32_t generic_write(int32_t fd, const uint8_t* k_buf, int32_t nbytes) {
    // sanity checks
    if (fd < 0 || fd >= MAX_NUM_FD) {return -1;}

    pcb_t* curr_pcb = get_current_pcb();
    if (!curr_pcb) {return -1;}
    const uint8_t* buf = (const uint8_t*)translate_user_to_kernel(k_buf, curr_pcb->pid);
    if (!buf) return -1;

    if (curr_pcb->fd_array[fd].present == 0 ||                  // the fdt is not present
        curr_pcb->fd_array[fd].operations == NULL ||            // the fdt's operation struct doesn't exist
        curr_pcb->fd_array[fd].operations->write == NULL        // the fdt's write operation doesn't exist
        ) {return -1;}

    return (*curr_pcb->fd_array[fd].operations->write)(&(curr_pcb->fd_array[fd].context), buf, nbytes);
}

/*
 * get_allocatable_fd
 *     DESCRIPTION: Iterate through the fd_array in the input pcb, return
 *                  an index of available file descriptor entry if any.
 *     INPUTS: pcb -- pointer to a input pcb.
 *     RETURN VALUE: index to a fdt upon success, NULL upon failure.
 */
static int32_t get_allocatable_fd(pcb_t* pcb) {
    int32_t ret_fd = FAIL_FD;
    int32_t i;
    for (i = 2; i < MAX_NUM_FD; i++) {
        if (pcb->fd_array[i].present) {continue;}
        ret_fd = i;
        break;
    }
    return ret_fd;
}

/*
 * initialize_fd_array
 *     DESCRIPTION: Initialize all the entries of the input fd array. Set up
 *                  default fd entires- stdin & stdout.
 *     INPUTS: fd_array -- pointer to a input fd_array.
 *     RETURN VALUE: none
 */
void initialize_fd_array(file_descriptor_t* fd_array) {
    int32_t i;

    // null check
    if (!fd_array) {return;}

    // initialize stdin fdt
    fd_array->operations = &stdin_ops;
    fd_array->context.filetype = FILETYPE_DEV;
    fd_array->context.inode = 0;
    fd_array->context.offset = 0;
    fd_array->present = 1;
    
    // initialize stdout fdt
    (fd_array + 1)->operations = &stdout_ops;
    (fd_array + 1)->context.filetype = FILETYPE_DEV;
    (fd_array + 1)->context.inode = 0;
    (fd_array + 1)->context.offset = 0;
    (fd_array + 1)->present = 1;

    // initialize unused fdts, fd 2-7
    for (i = 2; i < MAX_NUM_FD; i++) {
        (fd_array + i)->operations = NULL;
        (fd_array + i)->context.filetype = FILETYPE_UNKOWN;
        (fd_array + i)->context.inode = 0;
        (fd_array + i)->context.offset = 0;
        (fd_array + i)->present = 0;
    }
}

/*
 * close_fd
 *     DESCRIPTION: Clear all the values within the input fdt, except for
 *                  its fd number.
 *     INPUTS: fdt -- pointer to a input fdt.
 *     RETURN VALUE: none
 */
void close_fd(file_descriptor_t* fdt) {
    // null check
    if (!fdt) {return;}

    fdt->operations = NULL;
    fdt->context.filetype = FILETYPE_UNKOWN;
    fdt->context.inode = 0;
    fdt->context.offset = 0;
    fdt->present = 0;
}
