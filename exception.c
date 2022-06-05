#include "x86_desc.h"
#include "lib.h"
#include "paging.h"
#include "process/process.h"
#include "types.h"

#define DEATH_BY_EXCEPTION_CODE 256
// #define DEBUG_MSG

void unrecoverable_message(char* msg, hwcontext_t* context);

// Common exception handler. 
// Inputs: Object of type hwcontext_t, the context of the handler.
// Output: None
// Side effect: Depends on the vector number of the context.
void common_exception_handler(hwcontext_t* context) {
    set_new_cr3((uint32_t)kernel_page_descriptor_table);
    if (context->iret_context.cs == KERNEL_CS) {
        unrecoverable_message("Crash from kernel!", context);
    } else {
        // Attempt to obtain variables necessary to handle user-level exceptions
        // If any of these conditions fail we give up
        pcb_t* curr_pcb = get_current_pcb();
        if (!curr_pcb) {
            unrecoverable_message("Null current PCB!", context);
        }
        uint32_t this_pid = curr_pcb->pid;
        if (is_kernel_pid(this_pid)) {
            unrecoverable_message("PID 0 crashed, giving up!", context);
        }
        pcb_t* parent_pcb = get_pcb(curr_pcb->parent_pid);
        if (!parent_pcb) {
            unrecoverable_message("No parent to return to, giving up!", context);
        }
        uint32_t return_to_pid = parent_pcb->pid;

        #ifdef DEBUG_MSG
        printf("Return to %d\n", return_to_pid);
        #endif 

        // Set the CPU in this stack to the parent's CPU state
        // (ret_eip is automatically incremented for us by the x86 processor)
        *context = parent_pcb->pre_sysexec_state;
        
        // Set return value
        context->eax = DEATH_BY_EXCEPTION_CODE; 

        // See note in sys_halt_helper on restoring tss.esp0 
        tss.esp0 = get_initial_esp0_of_process(return_to_pid);

        // Do cleanup of dead process paging
        process_free(this_pid);

        destroy_user_programpage(
            this_pid
        );
        
        // If necessary set up parent vidmap paging
        if (parent_pcb->flag_activated_vidmap) {
            activate_user_vidmem();
        } else { 
            deactivate_user_vidmem();
        }
        
        activate_existing_user_programpage(
            return_to_pid
        );

        #ifdef DEBUG_MSG
        printf("New context:\n");
        dump_context(*context);
        #endif

        set_new_cr3((uint32_t)user_page_descriptor_table);
    }
}

// Prints a message about the exception, as well as the hardware context passed in
// Inputs: msg: String to print, contetxt: State of the processer on exception
// Outputs: None
// Side effects: Print debug information to the screen and busy loops
void unrecoverable_message(char* msg, hwcontext_t* context) {
    printf(msg);
    printf("\n");
    dump_context(*context);
    switch (context->vecnum) {
        case (IDT_DIVERR):
            printf("Division error!   \n");
            break;
        case (IDT_INTEL_RESERVED):
            printf("Intel Reserved (1)!   \n");
            break;
        case (IDT_NMIINT):
            printf("NMI Interrupt!   \n");
            break;
        case (IDT_BREAK):
            printf("Breakpoint!   \n");
            break;
        case (IDT_OVERFLOW):
            printf("Overflow!   \n");
            break;
        case (IDT_BOUND):
            printf("BOUND range exceeded!   \n");
            break;
        case (IDT_INVALOP):
            printf("Invalid Opcode (Undefined Opcode)!   \n");
            break;
        case (IDT_DEVICENA):    
            printf("Device Not Available (No Math Coprocessor) !   \n");
            break;
        case (IDT_DOUBLEFAULT):
            printf("Double Fault!    \n");
            break;
        case (IDT_SEGMENT_OVERRUN_RESERVED):
            printf("Segment Overrun Reserved (9)!    \n");
            break;
        case (IDT_INVALTSS):
            printf("Invalid TSS!    \n");
            break;
        case (IDT_SEGNOTPRESENT):
            printf("Segment Not Present!    \n");
            break;
        case (IDT_STACKSEGFAULT):
            printf("Stack-Segment Fault!    \n");
            break;
        case (IDT_GENPROTECT):
            printf("General Protection!    \n");
            break;
        case (IDT_PAGEFAULT):
            printf("Page fault!    \n");
            uint32_t pf_addr;
            asm (
                "movl %%cr2, %%eax;"
                "movl %%eax, %[addr]"
                : [addr] "=m" (pf_addr)
                :
                : "eax"
            );
            cr3_register_fmt cr3_value;
            printf("Violating address: %#x\n", pf_addr);
            asm (
                "movl %%cr3, %%eax;"
                "movl %%eax, %[cr3_value];"
                : [cr3_value] "=m" (cr3_value.bits)
                :
                : "eax"
            );
            uint32_t addr = cr3_value.page_directory_base << 12;
            printf("Active Cr3: ");
            if (addr == GET_20_MSB((uint32_t)user_page_descriptor_table) << 12) {
                printf("User");
            } else if (addr == GET_20_MSB((uint32_t)kernel_page_descriptor_table) << 12) {
                printf("Kernel");
            }
            break;
        case (IDT_INTEL_RESERVED_15):
            printf("Intel Reserved (15)!    \n");
            break;
        case (IDT_MATHFAULT):
            printf("x87 FPU Floating-Point Error (Math Fault)!    \n");
            break;
        case (IDT_ALIGNCHK):
            printf("Alignment Check!    \n");
            break;
        case (IDT_MACHINECHK):
            printf("Machine Check!    \n");
            break;
        case (IDT_SIMDFPE):
            printf("SIMD Floating Point Error!    \n");
            break;
    }

    printf("                   \n");
    printf("                   \n");
    printf("                   \n");
    printf("                   \n");
    
    SPIN();
}
