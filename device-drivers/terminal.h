#ifndef _TERMINAL_H_
#define _TERMINAL_H_

#include "../types.h"
#include "../lib.h"
#include "../process/file.h"
#include "i8259.h"
#include "keyboard.h"

#define MAX_NUM_TERMINAL    3
#define TERMIANL1_ID    0
#define TERMIANL2_ID    1
#define TERMIANL3_ID    2

typedef struct {
    int32_t tid;
    keyboard_context_t kb_context;
    int32_t screen_x;
    int32_t screen_y;
    int8_t* vmem_begin_addr;
    volatile int32_t is_reading;
} terminal_t;

extern int32_t displayed_tid;
extern int32_t active_tid;
extern terminal_t terminals[MAX_NUM_TERMINAL];

/* stdin & stdout operations */
extern file_operations_t stdin_ops;
extern file_operations_t stdout_ops;

extern void terminal_init(void);

extern void set_displayed_terminal(int32_t tid);
extern int32_t set_active_terminal(int32_t tid);

extern void vmem_save(int32_t tid);
extern void vmem_load(int32_t tid);

extern terminal_t* get_terminal(int32_t tid);
extern int32_t is_valid_tid(int32_t tid);

/* Open a termianl */
extern int32_t terminal_open  (void);
/* Close a ternimal */
extern int32_t terminal_close (void);
/* Read keyboard input from user through a ternimal, and display */
extern int32_t terminal_read  (file_context* fc, uint8_t* buf, int32_t nbytes);
/* Write strings through a terminal to display */
extern int32_t terminal_write (file_context* fc, const uint8_t* buf, int32_t nbytes);

#endif
