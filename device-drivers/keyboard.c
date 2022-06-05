#include "keyboard.h"
#include "terminal.h"
#include "../lib.h"

/*
Reference:
https://github.com/torvalds/linux/blob/master/drivers/input/keyboard/atkbd.c
*/

// an array that holds all the possible keyboard to character mapping
static uint8_t scancode_to_keycode[NUM_MODE][NUM_KEY] = {
	// state 1, no caps or shift pressed
	{'\0', '\0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\0', '\t',
	 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', '\0', 'a', 's',
	 'd', 'f', 'g', 'h', 'j', 'k', 'l' , ';', '\'', '`', '\0', '\\', 'z', 'x', 'c', 'v', 
	 'b', 'n', 'm',',', '.', '/', '\0', '*', '\0', ' ', '\0', '\0', '\0', '\0'},
	// state 2, shift pressed
	{'\0', '\0', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\0', '\t',
	 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', '\0', 'A', 'S',
	 'D', 'F', 'G', 'H', 'J', 'K', 'L' , ':', '"', '~', '\0', '|', 'Z', 'X', 'C', 'V', 
	 'B', 'N', 'M', '<', '>', '?', '\0', '*', '\0', ' ', '\0', '\0', '\0', '\0'},
	// state 3, caps pressed
	{'\0', '\0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\0', '\t',
	 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\n', '\0', 'A', 'S',
	 'D', 'F', 'G', 'H', 'J', 'K', 'L' , ';', '\'', '`', '\0', '\\', 'Z', 'X', 'C', 'V', 
	 'B', 'N', 'M', ',', '.', '/', '\0', '*', '\0', ' ', '\0', '\0', '\0', '\0'},
	// state 4, caps and shift pressed
	{'\0', '\0', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\0', '\t',
	 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '{', '}', '\n', '\0', 'a', 's',
	 'd', 'f', 'g', 'h', 'j', 'k', 'l' , ':', '"', '~', '\0', '\\', 'z', 'x', 'c', 'v', 
	 'b', 'n', 'm', '<', '>', '?', '\0', '*', '\0', ' ', '\0', '\0', '\0', '\0'}
};

/* states for capslock */
enum capslock_state {CL_LOWERCASE, CL_UPPERCASE};

/* file scope variables */
static uint32_t stk_mode_idx;
static uint32_t ctrl_pressed;
static uint32_t shift_pressed;
static uint32_t alt_pressed;
static uint32_t l_up_edge;
static enum capslock_state curr_cl_state;

/* file scope functions */
int handle_func_key(uint8_t scancode);
int kb_buf_add(uint8_t c, keyboard_context_t* kb_context);
uint8_t kb_buf_remove(keyboard_context_t* kb_context);
void kb_buf_print(keyboard_context_t* kb_context);

/*
 * keyboard_init
 *     DESCRIPTION: This function initializes keyboard interrupt by simply
 *					enabling IRQ 1 on PIC.
 *     INPUTS: none
 *     RETURN VALUE: none
 *     SIDE EFFECTS: modifies the state of PIC.
 */
void keyboard_init() {
	// NOTE: may need to read from keyboard to determine the curr_cl_state
	stk_mode_idx = 0;
	ctrl_pressed = 0;
	shift_pressed = 0;
	alt_pressed = 0;
	l_up_edge = 1;
	curr_cl_state = CL_LOWERCASE;

    enable_irq(KEYBOARD_IRQ);
}

/*
 * keyboard_interrupt_handler
 *     DESCRIPTION: This is the handler function for keyboard interrupt. This 
 *		exists only to set the active terminal to the displayed, and then to undo it.
 *     INPUTS: none
 *     RETURN VALUE: none
 *     SIDE EFFECTS: none
 */
void keyboard_interrupt_handler_wrapped();
void keyboard_interrupt_handler() {
	uint32_t old_tid = active_tid;
	set_active_terminal(displayed_tid);
	keyboard_interrupt_handler_wrapped();
	set_active_terminal(old_tid);
}

/*
 * keyboard_interrupt_handler_wrapped
 *     DESCRIPTION: This is the actual handler function for keyboard interrupt.
 * 					The function read in scancode code from port 0x60, translate
 * 					scancode into keycode, and echo to screen.
 *     INPUTS: none
 *     RETURN VALUE: none
 *     SIDE EFFECTS: none
 */
void keyboard_interrupt_handler_wrapped() {
    uint8_t scancode;		// variable holding the scancode from kb port
    uint8_t keycode;		// variable holding the translated keycode from scancode

	// read key-pressed scancode from kb data port
    scancode = inb(KEYBOARD_PORT_DATA);
	terminal_t* displayed_terminal = get_terminal(displayed_tid);
	if (!displayed_terminal) {return;}
	keyboard_context_t* kb_context = &(displayed_terminal->kb_context);

/*----- termianl read INDEPENDENT section -----*/

	// handle functional keys: SHIFT, CTRL, ALT, CAPS, F1, F2, F3 
	// which only modifies file-scope variables/flags
	if (!handle_func_key(scancode)) {return;}

	// check for ctrl + l
	if (scancode == SCODE_P_CHAR_L && ctrl_pressed && l_up_edge) {
		l_up_edge = 0;
		// reset screen
		clear();
		reset_cursor();
		if (displayed_terminal->is_reading) {
			puts("391OS> ");
			kb_buf_print(kb_context);}
		return;
	}

	// ignore unused key-released scancode
 	if (scancode >= NUM_KEY) {return;}
	// translate scancode to keycode
	keycode = scancode_to_keycode[stk_mode_idx][scancode];

/*----- termianl read DEPENDENT section -----*/

	if (!displayed_terminal->is_reading) {return;}

	// handle functional keys: ENTER, BACKSPACE
	switch (scancode)
	{
	// enter pressed
	case SCODE_P_ENTER:
		displayed_terminal->is_reading = 0;
		break;
	// backspace pressed
	case SCODE_P_BACKSPACE:
		keycode = kb_buf_remove(kb_context);
		switch (keycode)
		{
		case '\0':
			break;
		case '\t':
			takec();
			takec();
			takec();
			takec();
			break;
		default:
			takec();
			break;
		}
		keycode = '\0'; 
		break;
	default:
		break;
	}
 	
	// echo keycode to screen
	if (keycode == '\0') {return;}
	if (!kb_buf_add(keycode, kb_context)) {putc(keycode);}
}

/*
 * handle_func_key
 *     DESCRIPTION: This function sets the values of functional key flags,
 * 					when detecting a corresponding functional key press or release. 
 *     INPUTS: scancode -- input scancode sent from keyboard.
 *     RETURN VALUE: 0 -- signals the scancode sent-in corresponds to a functional key.
 * 					-1 -- signals the scancode sent-in does not correspond to a functional key.
 *     SIDE EFFECTS: modifies kb buffer, video mem (for some cases), VGA regs (for some cases).
 */
int handle_func_key(uint8_t scancode) {
	switch (scancode)
	{
	// left shift pressed
	case SCODE_P_LSHIFT:
		shift_pressed = 1;
		if (curr_cl_state == CL_LOWERCASE) {stk_mode_idx = 1;}
		else {stk_mode_idx = 3;}
		return 0;
	// right shift pressed
	case SCODE_P_RSHIFT:
		shift_pressed = 1;
		if (curr_cl_state == CL_LOWERCASE) {stk_mode_idx = 1;}
		else {stk_mode_idx = 3;}
		return 0;
	// left shift released
	case SCODE_R_LSHIFT:
		shift_pressed = 0;
		if (curr_cl_state == CL_LOWERCASE) {stk_mode_idx = 0;}
		else {stk_mode_idx = 2;}
		return 0;
	// right shift released
	case SCODE_R_RSHIFT:
		shift_pressed = 0;
		if (curr_cl_state == CL_LOWERCASE) {stk_mode_idx = 0;}
		else {stk_mode_idx = 2;}
		return 0;
	// control key is pressed (either left/right)
	case SCODE_P_CTRL:
		ctrl_pressed = 1;
		return 0;
	// control key is released (either left/right)
	case SCODE_R_CTRL:
		ctrl_pressed = 0;
		return 0;
	// alt key is pressed
	case SCODE_P_ALT:
		alt_pressed = 1;
		return 0;
	// alt key is released
	case SCODE_R_ALT:
		alt_pressed = 0;
		return 0;
	// capslock pressed
	case SCODE_P_CAPSLOCK:
		// change current capslock state accordingly
		// set stk_mode_idx accordingly
		if (curr_cl_state == CL_LOWERCASE) {
			curr_cl_state = CL_UPPERCASE;
			if (shift_pressed) {stk_mode_idx = 3;}
			else {stk_mode_idx = 2;}
		} else {
			curr_cl_state = CL_LOWERCASE;
			if (shift_pressed) {stk_mode_idx = 1;}
			else {stk_mode_idx = 0;}
		}
		return 0;
	case SCODE_P_F1:
		if (alt_pressed) {set_displayed_terminal(0);}
		return 0;
	case SCODE_P_F2:
		if (alt_pressed) {set_displayed_terminal(1);}
		return 0;
	case SCODE_P_F3:
		if (alt_pressed) {set_displayed_terminal(2);}
		return 0;
	case SCODE_R_CHAR_L:
		l_up_edge = 1;
		return 0;
	default:
		return -1;
	}
}

/*
 * kb_buf_read
 *     DESCRIPTION: Copies the values in keyboard buffer into the input buffer.
 *     INPUTS: buf -- input buffer to be copied to.
 * 			nbytes -- number of bytes to be copied.
 *     RETURN VALUE: actual number of bytes read from keyboard buffer.
 *     SIDE EFFECTS: none.
 */
int kb_buf_read(void* buf, unsigned int nbytes, keyboard_context_t* kb_context) {
	uint8_t* kb_buf = kb_context->kb_buf;
	uint32_t kb_buf_idx = (uint32_t)(kb_context->kb_buf_idx);

	if (nbytes > kb_buf_idx) {
		memcpy(buf, (const void*) kb_buf, kb_buf_idx);
		return kb_buf_idx;
	} else {
		kb_buf[nbytes - 1] = '\n';
		memcpy(buf, (const void*) kb_buf, nbytes);
		return nbytes;
	}
}

/*
 * kb_buf_clear
 *     DESCRIPTION: Clear the keyboard buffer.
 *     INPUTS: none
 *     RETURN VALUE: none
 *     SIDE EFFECTS: none
 */
void kb_buf_clear(keyboard_context_t* kb_context) {
	kb_context->kb_buf_idx = 0;
}

/*
 * kb_buf_add
 *     DESCRIPTION: Add a character into the keyboard buffer.
 *     INPUTS: c -- character to be added.
 *     RETURN VALUE: 0 if succeeded, -1 if failed.
 *     SIDE EFFECTS: none.
 */
int kb_buf_add(uint8_t c, keyboard_context_t* kb_context) {
	uint8_t* kb_buf = kb_context->kb_buf;
	uint32_t kb_buf_idx = (uint32_t)(kb_context->kb_buf_idx);

	// NOTE: may need to consider tab
	if (kb_buf_idx < 0 || kb_buf_idx >= KEYBOARD_BUF_SIZE) {return -1;}
	if (kb_buf_idx >= KEYBOARD_BUF_SIZE - 1) {
		// last entry is reserved for '\n'
		if (c == '\n') {
			kb_buf[kb_buf_idx] = c;
			(kb_context->kb_buf_idx)++;
			return 0;
		} else {return -1;}
	}
	kb_buf[kb_buf_idx] = c;
	(kb_context->kb_buf_idx)++;
	return 0;
}

/*
 * kb_buf_remove
 *     DESCRIPTION: Remove a character from the keyboard buffer.
 *     INPUTS: none
 *     RETURN VALUE: the character removed, '\0' when the buffer is empty.
 */
uint8_t kb_buf_remove(keyboard_context_t* kb_context) {
	if (kb_context->kb_buf_idx <= 0) {return '\0';}
	(kb_context->kb_buf_idx)--;
	return kb_context->kb_buf[kb_context->kb_buf_idx];
}

/*
 * kb_buf_print
 *     DESCRIPTION: Print all charactors in keyboard buffer to screen.
 *     INPUTS: none
 *     RETURN VALUE: none
 */
void kb_buf_print(keyboard_context_t* kb_context) {
	uint8_t* kb_buf = kb_context->kb_buf;
	uint32_t kb_buf_idx = (uint32_t)(kb_context->kb_buf_idx);

	int i;
	for (i = 0; i < kb_buf_idx; i++) {
		putc(kb_buf[i]);
	}
}
