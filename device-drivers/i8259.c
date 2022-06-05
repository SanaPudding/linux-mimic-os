/* i8259.c - Functions to interact with the 8259 interrupt controller
 * vim:ts=4 noexpandtab
 */

#include "i8259.h"
#include "../lib.h"

/* Interrupt masks to determine which interrupts are enabled and disabled */
uint8_t master_mask; /* IRQs 0-7  */
uint8_t slave_mask;  /* IRQs 8-15 */

/*
 * i8259_init
 *     DESCRIPTION: Initialize both the master and slave 8259 PICs, set them
 *                  to cascade mode, set slave INTR on master IRQ 2, enable
 *                  slave interrupt and mask all other interrupts.
 *     INPUTS: none
 *     RETURN VALUE: none
 *     SIDE EFFECTS: modifies the state of PIC.
 */
void i8259_init(void) {

    //send ICW1
    outb(ICW1, MASTER_8259_PORT_CMD);
    outb(ICW1, SLAVE_8259_PORT_CMD);

    //send ICW2
    outb(ICW2_MASTER, MASTER_8259_PORT_DATA);
    outb(ICW2_SLAVE, SLAVE_8259_PORT_DATA);

    //send ICW3
    outb(ICW3_MASTER, MASTER_8259_PORT_DATA);
    outb(ICW3_SLAVE, SLAVE_8259_PORT_DATA);

    //send ICW4
    outb(ICW4, MASTER_8259_PORT_DATA);
    outb(ICW4, SLAVE_8259_PORT_DATA);

    //initialize local variables representing irq mask
    //0xFF represents mask all interrupts
    master_mask = 0xFF;
    slave_mask = 0xFF;

    //send OCW1 (Operation Control Word) to set interrupt mask
    outb(master_mask, MASTER_8259_PORT_DATA);
    outb(slave_mask, SLAVE_8259_PORT_DATA);

    //enable sPIC irq
    enable_irq(SLAVE_IRQ);

}

/*
 * enable_irq
 *     DESCRIPTION: Enable (unmask) the specified IRQ, 0-7 on master
 *                  PIC, 8-15 on slave PIC.
 *     INPUTS: irq_num -- the number of the IRQ line to be enabled
 *     RETURN VALUE: none
 *     SIDE EFFECTS: modifies the state of PIC.
 */
void enable_irq(uint32_t irq_num) {

    //check if irq_num is valid, irq_num is within [0, 15]
    //only need to check the upper bound since irq_num is unsigned
    if (irq_num > MAX_IRQ) return;

    //temp var for a irq mask
    uint8_t mask = 0x01;

    //the specified IRQ is on master PIC, irq_num = 0-7
    if (irq_num <= MAX_MASTER_IRQ) {
        mask <<= irq_num;
        master_mask &= (~mask);
        outb(master_mask, MASTER_8259_PORT_DATA);
    }
    //the specified IRQ is on slave PIC, irq_num = 8-15
    else {
        mask <<= (irq_num - 8);
        slave_mask &= (~mask);
        outb(slave_mask, SLAVE_8259_PORT_DATA);
    }

}

/*
 * disable_irq
 *     DESCRIPTION: Disable (mask) the specified IRQ, 0-7 on master
 *                  PIC, 8-15 on slave PIC.
 *     INPUTS: irq_num -- the number of the IRQ line to be disabled
 *     RETURN VALUE: none
 *     SIDE EFFECTS: modifies the state of PIC.
 */
void disable_irq(uint32_t irq_num) {

    //check if irq_num is valid, irq_num is within [0, 15]
    //only need to check the upper bound since irq_num is unsigned
    if (irq_num > MAX_IRQ) return;

    //temp var for a irq mask
    uint8_t mask = 0x01;

    //the specified IRQ is on master PIC, irq_num = 0-7
    if (irq_num <= MAX_MASTER_IRQ) {
        mask <<= irq_num;
        master_mask |= mask;
        outb(master_mask, MASTER_8259_PORT_DATA);
    }
    //the specified IRQ is on slave PIC, irq_num = 8-15
    else {
        mask <<= (irq_num - 8);
        slave_mask |= mask;
        outb(slave_mask, SLAVE_8259_PORT_DATA);
    }

}

/*
 * send_eoi
 *     DESCRIPTION: Send end-of-interrupt signal for the specified IRQ,
 *                  0-7 on master PIC, 8-15 on slave PIC.
 *     INPUTS: irq_num -- the number of the IRQ line to send eoi
 *     RETURN VALUE: none
 *     SIDE EFFECTS: modifies the state of PIC.
 */
void send_eoi(uint32_t irq_num) {

    //check if irq_num is valid, irq_num is within [0, 15]
    //only need to check the upper bound since irq_num is unsigned
    if (irq_num > MAX_IRQ) return;

    //the specified IRQ is on master PIC, irq_num = 0-7
    if (irq_num <= MAX_MASTER_IRQ) {
        outb((EOI | irq_num), MASTER_8259_PORT_CMD);        // send EOI to master
    }
    //the specified IRQ is on slave PIC, irq_num = 8-15
    else {
        outb((EOI | (irq_num - 8)), SLAVE_8259_PORT_CMD);   // send EOI to slave
 		send_eoi(SLAVE_IRQ);                                // alse send EOI to master on slave_irq line
    }
}
