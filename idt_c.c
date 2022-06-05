#include "x86_desc.h"
#include "device-drivers/rtc.h"
#include "device-drivers/keyboard.h"
#include "device-drivers/i8259.h"
#include "device-drivers/pit.h"
#include "paging.h"
#include "common.h"
#include "idt.h"
#include "types.h"

void idt_init() {
    
    /********** Set up IDT **********/
    idt_desc_t default_idt_entry;
    
    default_idt_entry.present = 1;
    default_idt_entry.dpl = 0;
    default_idt_entry.seg_selector = KERNEL_CS;
    default_idt_entry.size = 1;
    default_idt_entry.reserved0 = 0;
    default_idt_entry.reserved1 = 1;
    default_idt_entry.reserved2 = 1;
    default_idt_entry.reserved3 = 0;
    default_idt_entry.reserved4 = 0;

    // set up exception IDT entrees
    int i;
    for (i = 0; i <= IDT_EXCPT_END; i++) {
        // Ignore reserved exception vectors
        idt[i] = default_idt_entry;
    }

    // set up interrupts IDT entrees
    for (i = IDT_INT_START; i <= IDT_INT_END; i++) {
        idt[i] = default_idt_entry;
    }

    // set up system call IDT entrees
    idt[IDT_SYSCALL] = default_idt_entry;
    idt[IDT_SYSCALL].dpl = 0x3;
    idt[IDT_SYSCALL].reserved3 = 1;         // set system call entry as a trap gate

    SET_IDT_ENTRY(idt[IDT_DIVERR], IDT_ASM_WRAPPER(IDT_DIVERR));
    SET_IDT_ENTRY(idt[IDT_INTEL_RESERVED], IDT_ASM_WRAPPER(IDT_INTEL_RESERVED));
    SET_IDT_ENTRY(idt[IDT_NMIINT], IDT_ASM_WRAPPER(IDT_NMIINT));
    SET_IDT_ENTRY(idt[IDT_BREAK], IDT_ASM_WRAPPER(IDT_BREAK));
    SET_IDT_ENTRY(idt[IDT_OVERFLOW], IDT_ASM_WRAPPER(IDT_OVERFLOW));
    SET_IDT_ENTRY(idt[IDT_BOUND], IDT_ASM_WRAPPER(IDT_BOUND));
    SET_IDT_ENTRY(idt[IDT_INVALOP], IDT_ASM_WRAPPER(IDT_INVALOP));
    SET_IDT_ENTRY(idt[IDT_DEVICENA], IDT_ASM_WRAPPER(IDT_DEVICENA));
    SET_IDT_ENTRY(idt[IDT_DOUBLEFAULT], IDT_ASM_WRAPPER(IDT_DOUBLEFAULT));
    SET_IDT_ENTRY(idt[IDT_SEGMENT_OVERRUN_RESERVED], IDT_ASM_WRAPPER(IDT_SEGMENT_OVERRUN_RESERVED));
    SET_IDT_ENTRY(idt[IDT_INVALTSS], IDT_ASM_WRAPPER(IDT_INVALTSS));
    SET_IDT_ENTRY(idt[IDT_SEGNOTPRESENT], IDT_ASM_WRAPPER(IDT_SEGNOTPRESENT));
    SET_IDT_ENTRY(idt[IDT_STACKSEGFAULT], IDT_ASM_WRAPPER(IDT_STACKSEGFAULT));
    SET_IDT_ENTRY(idt[IDT_GENPROTECT], IDT_ASM_WRAPPER(IDT_GENPROTECT));
    SET_IDT_ENTRY(idt[IDT_PAGEFAULT], IDT_ASM_WRAPPER(IDT_PAGEFAULT));
    SET_IDT_ENTRY(idt[IDT_INTEL_RESERVED_15], IDT_ASM_WRAPPER(IDT_INTEL_RESERVED_15));
    SET_IDT_ENTRY(idt[IDT_MATHFAULT], IDT_ASM_WRAPPER(IDT_MATHFAULT));
    SET_IDT_ENTRY(idt[IDT_ALIGNCHK], IDT_ASM_WRAPPER(IDT_ALIGNCHK));
    SET_IDT_ENTRY(idt[IDT_MACHINECHK], IDT_ASM_WRAPPER(IDT_MACHINECHK));
    SET_IDT_ENTRY(idt[IDT_SIMDFPE], IDT_ASM_WRAPPER(IDT_SIMDFPE));

    // link wrapper function for interrupts
    SET_IDT_ENTRY(idt[IDT_KEYBOARD], keyboard_interrupt_wrapper);
    SET_IDT_ENTRY(idt[IDT_RTC], rtc_interrupt_wrapper);
    SET_IDT_ENTRY(idt[IDT_PIT], idt_asm_wrapper_pit);

    // link wrapper function for sys calls
    SET_IDT_ENTRY(idt[IDT_SYSCALL], idt_asm_wrapper_syscall);

    lidt(idt_desc_ptr);
}

// Function to identify whether we were called from the kernel or not
// Inputs: context: Hardware context 
// Outputs: 1 if kernel, 0 if user, -1 if malformed context
int32_t was_called_from_kernel(hwcontext_t* context) {
    if (context->iret_context.cs == KERNEL_CS) return 1;
    else if (context->iret_context.cs == USER_CS) return 0;
    else return -1;
}

// Common interrupt handler. 
// Inputs: Object of type hwcontext_t, the context of the handler.
// Output: None
// Side effect: Depends on the vector number of the context.
void common_interrupt_handler(hwcontext_t* context) {
    uint32_t interrupted_cr3 = get_cr3();
    set_new_cr3((uint32_t)kernel_page_descriptor_table);
    uint32_t flags, garbage;
    switch(context->vecnum) {
        case (IDT_KEYBOARD):
            CRITICAL_SECTION_FLAGSAVE(flags, garbage) {
                keyboard_interrupt_handler();
            }
            send_eoi(KEYBOARD_IRQ);
            break;
        case (IDT_RTC):
            rtc_interrupt_handler();
            send_eoi(RTC_IRQ);
            break;
        default:
            return;
    }
    set_new_cr3(interrupted_cr3);
}
