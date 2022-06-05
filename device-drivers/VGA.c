#include "VGA.h"

// reference: https://wiki.osdev.org/Text_Mode_Cursor

/*
 * disable_cursor
 *     DESCRIPTION: Disable text mode VGA build-in cursor.
 *     INPUTS: none
 *     RETURN VALUE: none
 *     SIDE EFFECTS: modifies VGA CRT Control reg.
 */
void disable_cursor(void)
{
	outb(CURSOR_START_REG, VGA_PORT_CRTC_ADDR);	// select Cursor Start reg
	outb(0x20, VGA_PORT_CRTC_DATA);				// set Cursor Disable field to 1
}

/*
 * set_cursor_VGA
 *     DESCRIPTION: Update text mode VGA build-in cursor position on screen.
 *     INPUTS: (x, y) -- coordinate of the cursor
 *     RETURN VALUE: none
 *     SIDE EFFECTS: modifies VGA CRT Control regs.
 */
void set_cursor_VGA(int x, int y)
{
	// Cursor Location field is 16-bit
	uint16_t pos = y * VGA_WIDTH + x;

	// set bit 7-0 of the Cursor Location field
	outb(CURSOR_LOCATION_LOW_REG, VGA_PORT_CRTC_ADDR);
	outb((uint8_t) (pos & 0xFF), VGA_PORT_CRTC_DATA);

	// set bit 15-8 of the Cursor Location field
	outb(CURSOR_LOCATION_HIGH_REG, VGA_PORT_CRTC_ADDR);
	outb((uint8_t) ((pos >> 8) & 0xFF), VGA_PORT_CRTC_DATA);
}
