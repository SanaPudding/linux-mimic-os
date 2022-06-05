#include "../types.h"

#define PIT_IRQ                 0               //pit channel 0 uses IRQ 0
#define PIT_CHANNEL_ZERO_PORT   0x40            //channel 0 data port
#define PIT_MODE_REG            0x43            //needed to access mode 3
#define PIT_MODE_3              0x36        
#define FREQ                    1193182
#define LOWER_BITS              0xFF
#define TIME_SLICE_FREQUENCY    20

extern void pit_init();
void pit_interrupt_handler();
void idt_asm_wrapper_pit();
