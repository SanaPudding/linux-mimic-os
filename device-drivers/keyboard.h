#ifndef _KEYBOARD_H_
#define _KEYBOARD_H_

#include "i8259.h"

#define KEYBOARD_IRQ    1           // keyboard interrupt has irq_num 1

#define KEYBOARD_PORT_DATA	0x60    // PS/2 keyboard data port is at 0x60
#define KEYBOARD_PORT_CMD	0x64	// PS/2 keyboard command port is at 0x64

#define NUM_KEY     (SCODE_P_F3 + 1)
#define NUM_MODE	4

#define KEYBOARD_BUF_SIZE   128     // size of the keyboard buffer

/* 
 * Scancode set 1 for functional keys,
 * P indicates pressed, R indicates released 
 */
#define SCODE_P_LSHIFT      0x2A
#define SCODE_P_RSHIFT      0x36
#define SCODE_P_CTRL        0x1D
#define SCODE_P_ALT         0x38
#define SCODE_P_CAPSLOCK    0x3A
#define SCODE_P_TAB         0x0F
#define SCODE_P_ENTER       0x1C
#define SCODE_P_BACKSPACE   0x0E
#define SCODE_P_F1          0x3B
#define SCODE_P_F2          0x3C
#define SCODE_P_F3          0x3D

#define SCODE_R_LSHIFT      0xAA
#define SCODE_R_RSHIFT      0xB6
#define SCODE_R_CTRL        0x9D
#define SCODE_R_ALT         0xB8
#define SCODE_R_CAPSLOCK    0xBA
#define SCODE_R_TAB         0x8F
#define SCODE_R_ENTER       0x9C
#define SCODE_R_BACKSPACE   0x8E
#define SCODE_R_F1          0xBB
#define SCODE_R_F2          0xBC
#define SCODE_R_F3          0xBD

#define SCODE_P_CHAR_L      0x26    // scancode for letter "l/L" pressed
#define SCODE_R_CHAR_L      0xA6    // scancode for letter "l/L" released

#ifndef ASM

typedef struct {
    uint8_t kb_buf[KEYBOARD_BUF_SIZE];
    int32_t kb_buf_idx;
} keyboard_context_t;

/* Initialize keyboard interrupt */
extern void keyboard_init();
/* Handling keyboard interrupt */
extern void keyboard_interrupt_handler();
/* read the contents in keyboard buffer */
extern int kb_buf_read(void* buf, unsigned int nbytes, keyboard_context_t* kb_context);
/* Clear the keyboard buffer */
extern void kb_buf_clear(keyboard_context_t* kb_context);

#endif
#endif
