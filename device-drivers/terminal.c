#include "terminal.h"
#include "../paging.h"


/* operation structs for terminal */
file_operations_t stdin_ops = {
    .open = fd_open_noop,
    .close = fd_close_noop,
    .read = terminal_read,
    .write = fd_write_noop
};

file_operations_t stdout_ops = {
    .open = fd_open_noop,
    .close = fd_close_noop,
    .read = fd_read_noop,
    .write = terminal_write
};

int32_t displayed_tid;
int32_t active_tid;
terminal_t terminals[MAX_NUM_TERMINAL];

void vmem_save(int32_t tid);
void vmem_load(int32_t tid);

/* dummy functions, no real functionality */
int32_t terminal_open(void) {return 0;}
int32_t terminal_close(void) {return 0;}

/*
 * terminal_read
 *     DESCRIPTION: This function reads nbytes of input characters from keyboard,
 *                  store those chars into the input buf, and return the actual
 *                  number of bytes read.
 *     INPUTS:  fd -- file descriptor.
 *             buf -- the buffer receiving all the bytes read.
 *          nbytes -- number of bytes to read in from keyborad.
 *     RETURN VALUE: number of bytes actually read from keyboard buffer.
 *     SIDE EFFECTS: modifies video memory, resets keyboard buffer.
 */
int32_t terminal_read(file_context* fc, uint8_t* buf, int32_t nbytes) {
    // null check
    terminal_t* active_terminal = get_terminal(active_tid);
    if (!buf || !active_terminal) {return -1;}
    keyboard_context_t* kb_context = &(active_terminal->kb_context);

    // clear keyboard buffer, and set terminal_reading flag
    kb_buf_clear(kb_context);
    active_terminal->is_reading = 1;
    while (active_terminal->is_reading) {/* waiting for ENTER key */};
    return kb_buf_read(buf, nbytes, kb_context);
}

/*
 * terminal_write
 *     DESCRIPTION: This function writes nbytes of characters from the input
 *                  buffer onto screen display.
 *     INPUTS:  fd -- file descriptor.
 *             buf -- the buffer holding all the chars to be written.
 *          nbytes -- number of bytes to be written.
 *     RETURN VALUE: number of bytes written.
 *     SIDE EFFECTS: modifies video memory.
 */
int32_t terminal_write(file_context* fc, const uint8_t* buf, int32_t nbytes) {
    // null check
    if (!buf) {return -1;}
    
    // print chars in buf to screen
    int i;
    for (i = 0; i < nbytes; i++) {
        putc(*((uint8_t*) buf + i));
    }
    return nbytes;
}

/*
 * terminal_init
 *     DESCRIPTION: Initialize statically allocated terminal structs.
 *     INPUTS: none.
 *     RETURN VALUE: none.
 */
void terminal_init(void) {
    int i;
    for (i = 0; i < MAX_NUM_TERMINAL; i++) {
        terminals[i].tid = i;
        terminals[i].kb_context.kb_buf_idx = 0;
        terminals[i].screen_x = 0;
        terminals[i].screen_y = 0;
        terminals[i].is_reading = 0;
    }
    terminals[TERMIANL1_ID].vmem_begin_addr = (int8_t*) KERN_VMEM_PHYSICAL_BEGIN_ADDR;
    terminals[TERMIANL2_ID].vmem_begin_addr = (int8_t*) BACKGROUND_VMEM_PHYSICAL_BEGIN_ADDR_T2;
    terminals[TERMIANL3_ID].vmem_begin_addr = (int8_t*) BACKGROUND_VMEM_PHYSICAL_BEGIN_ADDR_T3;

    displayed_tid = TERMIANL1_ID;
    active_tid = TERMIANL1_ID;
}

/*
 * set_displayed_terminal
 *     DESCRIPTION: This function is responsible for terminal switch when alt+f1/2/3
 *                  is pressed. The function loads the screen contents of terminal being
 *                  switched to.
 *     INPUTS: tid -- terminal id of the target terminal
 *     RETURN VALUE: none.
 */
void set_displayed_terminal(int32_t tid) {
    // sanity checks
    if (!is_valid_tid(tid)) {return;}
    if (displayed_tid == tid) {return;}

    int32_t curr_tid = displayed_tid;
    int32_t next_tid = tid;
    terminal_t* curr_terminal = get_terminal(curr_tid);
    terminal_t* next_terminal = get_terminal(next_tid);
    terminal_t* active_terminal = get_terminal(active_tid);
    if (!curr_terminal || !next_terminal || !active_terminal) {return;}

    curr_terminal->vmem_begin_addr = (int8_t*) get_default_bgvmem_begin_addr(curr_tid);
    next_terminal->vmem_begin_addr = (int8_t*) KERN_VMEM_PHYSICAL_BEGIN_ADDR;

    // save old vmem, load new vmem
    // modify display only
    vmem_save(curr_tid);
    vmem_load(next_tid);
    set_cursor_VGA(next_terminal->screen_x, next_terminal->screen_y);
    displayed_tid = next_tid;

    // let active terminal printing to the correct vmem page
    set_lib_vmem_pointer(active_terminal->vmem_begin_addr);
    return;
}

/*
 * set_active_terminal
 *     DESCRIPTION: This function is responsible for terminal switch between
 *                  processes corresponding to different terminals.
 *     INPUTS: tid -- terminal id of the target terminal.
 *     RETURN VALUE: 0 upon success, -1 upon failure.
 */
int32_t set_active_terminal(int32_t tid) {
    // sanity checks
    if (!is_valid_tid(tid)) {return -1;}
    if (active_tid == tid) {return 0;}

    int32_t curr_tid = active_tid;
    int32_t next_tid = tid;
    terminal_t* curr_terminal = get_terminal(curr_tid);
    terminal_t* next_terminal = get_terminal(next_tid);
    if (!curr_terminal || !next_terminal) {return -1;}

    // kernel side vmem re-map
    save_cursor(&(curr_terminal->screen_x), &(curr_terminal->screen_y));
    load_cursor(next_terminal->screen_x, next_terminal->screen_y);
    set_lib_vmem_pointer(next_terminal->vmem_begin_addr);
    
    // user side vmem re-map
    set_user_vmem_base_addr((uint32_t) (next_terminal->vmem_begin_addr));

    // set new active tid, and corresponding vmem mapping
    active_tid = next_tid;
    return 0;
}

/*
 * vmem_save
 *     DESCRIPTION: This function save the currently displayed screen content
 *                  to a background(hidden) video memory area.
 *     INPUTS: tid -- terminal id that corresponds to a background vmem area.
 *     RETURN VALUE: none.
 */
void vmem_save(int32_t tid) {
    if (!is_valid_tid(tid)) {return;}
    const void* src_addr = (const void*) KERN_VMEM_PHYSICAL_BEGIN_ADDR;
    void* dest_addr = (void*) get_default_bgvmem_begin_addr(tid);
    memcpy(dest_addr, src_addr, VMEM_SIZE);
}

/*
 * vmem_load
 *     DESCRIPTION: This function load the contents of a background(hidden) 
 *                  video memory area onto screen display.
 *     INPUTS: tid -- terminal id that corresponds to a background vmem area.
 *     RETURN VALUE: none.
 */
void vmem_load(int32_t tid) {
    if (!is_valid_tid(tid)) {return;}
    const void* src_addr = (const void*) get_default_bgvmem_begin_addr(tid);
    void* dest_addr = (void*) KERN_VMEM_PHYSICAL_BEGIN_ADDR;
    memcpy(dest_addr, src_addr, VMEM_SIZE);
}

/*
 * get_terminal
 *     DESCRIPTION: Return a pointer to a terminal struct corresponding to the
 *                  input terminal id.
 *     INPUTS: tid -- a terminal id.
 *     RETURN VALUE: a pointer to a terminal struct upon success, null upon failure.
 */
terminal_t* get_terminal(int32_t tid) {
    if (!is_valid_tid(tid)) {return NULL;}
    return &terminals[tid];
}

/*
 * is_valid_tid
 *     DESCRIPTION: Check if the input terminal id is valid.
 *     INPUTS: tid -- a terminal id.
 *     RETURN VALUE: 1 if valid, 0 if not.
 */
int32_t is_valid_tid(int32_t tid) {
    if (tid < 0 || tid >= MAX_NUM_TERMINAL) {return 0;}
    return 1;
}
