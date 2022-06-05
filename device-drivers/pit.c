#include "pit.h"
#include "i8259.h"
#include "../lib.h"
#include "../common.h"

// Sets the PIT frequency in hertz.
// Inputs: hz
// Outputs: None
// Side effects: Sets the PIT hardware
void pit_set_freq(uint32_t hz) {
    const uint32_t MAGIC_PIT = 119180;
    uint32_t div = MAGIC_PIT / hz;
    outb(PIT_MODE_3, PIT_MODE_REG);
    outb(NTH_BYTE(0, div), PIT_CHANNEL_ZERO_PORT);
    outb(NTH_BYTE(1, div), PIT_CHANNEL_ZERO_PORT);
}

// Initalizes the PIT
// Inputs/Outputs: None
// Side effects: Initializes the PIT but does not start it.
void pit_init() {
    uint32_t flags, garbage;
    CRITICAL_SECTION_FLAGSAVE(flags, garbage) {
        pit_set_freq(TIME_SLICE_FREQUENCY);
        enable_irq(PIT_IRQ);
    }
}
