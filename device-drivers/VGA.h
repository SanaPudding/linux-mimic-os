#ifndef _VGA_H_
#define _VGA_H_

#include "../lib.h"

/* Screen dimension */
#define VGA_WIDTH   80
#define VGA_HEIGHT  25

/* CRT Control Registers ports */
#define VGA_PORT_CRTC_ADDR  0x3D4
#define VGA_PORT_CRTC_DATA  0x3D5

/* Indices for different CRT Control Registers */
#define CURSOR_START_REG            0x0A
#define CURSOR_LOCATION_HIGH_REG    0X0E
#define CURSOR_LOCATION_LOW_REG     0x0F

/* Disable text mode VGA cursor */
void disable_cursor(void);

/* Update text mode VGA cursor position */
void set_cursor_VGA(int x, int y);

#endif
