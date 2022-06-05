#include "rtc.h"
#include "i8259.h"
#include "../common.h"
#include "../lib.h"
#include "../tests/tests.h"
#include "../process/process.h"
#include "../sched/sched.h"

rtc_state_t process_clocks[NUM_SIMULTANEOUS_PROCS];
/* operation struct for rtc */
file_operations_t rtc_ops = {
    .open = rtc_open,
    .close = rtc_close,
    .read = rtc_read,
    .write = rtc_write
};

/* flag that interrupt rtc interrupt occurred */
uint32_t virt_rtc_clock;

/*
 * rtc_init
 *     DESCRIPTION: This function initializes rtc interrupt with 
 *     the IRQ number 8. 
 *     INPUTS: none
 *     RETURN VALUE: none
 *     SIDE EFFECTS: modifies the state of PIC and turns on the periodic interrupt.
 */
void rtc_init() {
    uint32_t flags, garbage;
    CRITICAL_SECTION_FLAGSAVE(flags, garbage) {
        // Initialize Sequence
        outb(REGISTER_B, RTC_PORT);     //select reg b and disable NMI
        char prevB = inb(CMOS_PORT);     //read the current value of B
        outb(REGISTER_B, RTC_PORT);     //read resets the index to reg d, so set the index again
        outb(prevB | 0x40, CMOS_PORT);   //turn on bit 6 of register B

        // Write the max possible frequency (Maybe?)
        outb(REGISTER_A,RTC_PORT);      //set index to reg A, disable NMI
        char prevA = inb(CMOS_PORT);     //get initial value of register A
        outb(REGISTER_A,RTC_PORT);      //reset index
        outb(((prevA & 0xF0) | 0x3) , CMOS_PORT);    //write rate to A
    }

    int i;
    for (i = 0; i < NUM_SIMULTANEOUS_PROCS; i++) {
        process_clocks[i].freq = 2;
        process_clocks[i].clock_strike_flag = 0;
    }
    
    enable_irq(RTC_IRQ);            //enable irq
}

/*
 * rtc_interrupt_handler
 *     DESCRIPTION: This is the actual handler function for rtc interrupt.
 *                  Whenever rtc sends an interrupt, it calls the test interrupts function
 *                  and reads from the register C to enable interrupt again.
 *     INPUTS: none
 *     RETURN VALUE: none
 *     SIDE EFFECTS: It prints out characters on the screen that changes at 
 *                   a certain frequency.
 */
void rtc_interrupt_handler() {
    // test_interrupts();
    // set interrupt flag to 1 to indicate it happened
    int i; 
    for (i = 0; i < NUM_SIMULTANEOUS_PROCS; i++) {
        // Tunable parameter
        #define RTC_CLOCK_MULTIPLIER 16
        uint32_t timer_modulo = (MAX_PHYSICAL_FREQ / process_clocks[i].freq / RTC_CLOCK_MULTIPLIER);
        if (!timer_modulo) timer_modulo = 1;
        if ((virt_rtc_clock % timer_modulo) == 0) {
            process_clocks[i].clock_strike_flag = 1;
        }
    }

    uint32_t flags, garbage;
    // If the below process does not happen, the interrupt will not happen again
    // https://wiki.osdev.org/RTC#Setting_the_Registers
    CRITICAL_SECTION_FLAGSAVE(flags, garbage) {
        virt_rtc_clock++;
        outb(0x0C, RTC_PORT);           //select reg c.
        inb(CMOS_PORT);                 //throw away contents
    }
}

/*
 * rtc_open
 *     DESCRIPTION: This is the function that sets the RTC frequency to 2hz.
 *     INPUTS: filename -- not used in checkpoint 2
 *     RETURN VALUE: 0
 *     SIDE EFFECTS: Sets the RTC frequency to 2hz and return 0
 */
int rtc_open(void) {
    // initialize frequency to 2hz
    rtc_set_freq(OPEN_FREQ);
    return 0;
}

/*
 * rtc_close
 *     DESCRIPTION: No real functionality for now..
 *     INPUTS: fd -- unused
 *     RETURN VALUE: default 0
 */
int rtc_close(void) {return 0;}

/*
 * rtc_read
 *     DESCRIPTION: This is the function blocks until the next interrupt 
 *                  is received.
 *     INPUTS: fc - File context of the FD
 *             buf - Ignored,
 *             nbytes - Ignored
 *     RETURN VALUE: 0
 *     SIDE EFFECTS: Waits for the next interrupt and return 0
 */
int rtc_read(file_context* fc, uint8_t* buf, int32_t nbytes){
    // block until the next rtc interrupt happens
    uint32_t clock_idx = get_canonical_pid(get_current_pid()) - 1;
    process_clocks[clock_idx].clock_strike_flag = 0;
    while (!process_clocks[clock_idx].clock_strike_flag);
    // printf("Unblocked RTC at frequency %d for PID %d\n", process_clocks[clock_idx].freq, get_current_pid());
    return 0;
}

/*
 * rtc_write
 *     DESCRIPTION: This is the function that takes const void pointer buf
 *                  that holds the frequency value and sets rtc to that value by calling helper function.
 *     INPUTS: fd -- unused,
 *             buf -- void pointer that holds the frequency to be set
 *             nbytes -- how many bytes the buf is
 *     RETURN VALUE: nbytes -- number of bytes written or -1 -- when buf is not in the boundary
 *     SIDE EFFECTS: RTC is set to the specified value.
 */
int rtc_write(file_context* fc, const uint8_t* buf, int32_t nbytes){
    int32_t freq = *((int32_t*) buf);
    // sanity check
    if (nbytes != 4) {return -1;}
    if (freq < 2 || freq > MAX_FREQ || freq & (freq-1)) {
        return -1;
    } else {
        //set rtc to the frequency
        rtc_set_freq(freq);
        return 0;
    }
}

/*
 * rtc_set_freq
 *     DESCRIPTION: Helper function to set the rtc to the specified frequency.
 *                  
 *     INPUTS: freq -- frequency to be set
 *     RETURN VALUE: 0
 *     SIDE EFFECTS: Sets the rtc registers to change the frequency.
 *                   
 */
void rtc_set_freq(int freq) {
    if (freq == 0) return;
    uint32_t flags, garbage;
    CRITICAL_SECTION_FLAGSAVE(flags, garbage) {
        uint32_t clock_idx = get_canonical_pid(get_current_pid()) - 1;
        process_clocks[clock_idx].freq = freq;
    }
}
