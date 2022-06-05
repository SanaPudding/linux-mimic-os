#ifndef _RTC_H
#define _RTC_H

// #include "../lib.h"
#include "../types.h"
#include "../process/file.h"

#define RTC_IRQ     8               //rtc irq number is defined as 8
#define RTC_PORT    0x70            //RTC port to specify index and disable NMI
#define CMOS_PORT   0x71            //CMOS Port used to read or write from the byte

//RTC status registers
#define REGISTER_A 0x8A
#define REGISTER_B 0x8B
#define REGISTER_C 0xC

#define OPEN_FREQ 0x2
#define MAX_FREQ 1024

#define MAX_PHYSICAL_FREQ 8192
#ifndef ASM

/* operation struct for rtc */
extern file_operations_t rtc_ops;

/*Initializes keyboard interrupt*/
extern void rtc_init();

/*Handles rtc interrupt*/
extern void rtc_interrupt_handler();

/* rtc open/close/read/write */
extern int rtc_open  (void);
extern int rtc_close (void);
extern int rtc_read  (file_context* fc, uint8_t* buf, int nbytes);
extern int rtc_write (file_context* fc, const uint8_t* buf, int nbytes);

/*Helper function to write frequency value to the RTC */
void rtc_set_freq(int freq);

typedef struct rtc_state_t {
    int freq;
    volatile int clock_strike_flag;
} rtc_state_t;
#endif
#endif
