#define ASM 1
#include "../idt.h"
#include "sched.h"

.globl handle_pit_interrupt, schedule_failed, exit_sched_to_k, exit_sched_to_u, exit_sched_to_k_helper, exit_sched_to_u_helper, idt_asm_wrapper_pit

temp_eax:
    .long 0x0

idt_asm_wrapper_pit:
    pushl %esp
    PUSHALL_REGS_HW_CONTEXT
    pushl %esp // Push pointer to struct
    call handle_pit_interrupt
    cmpl $-1, %eax
    jle _scheduling_fail

// ASM linkage function for the scheduler to return to the next process which is userland
// Inputs: None (not passed directly)
// Outputs: None (not passed directly)
// Side effects: Completes the scheduler servicing and IRETs to the next scheduled process in usermode
exit_sched_to_u:
    pushl %ebp
    movl %esp, %ebp
    subl $20, %esp // Allocate IRET to user (5 longs)
    subl $SIZEOF_PUSHALL, %esp // Allocate regs context
    pushl %esp
    call exit_sched_to_u_helper
    addl $4, %esp

    cmpl $-1, %eax
    jle _scheduling_fail
    
    POPALL_REGS_HW_CONTEXT // Restore
    iret // Kernel to user, returning is not that special

// ASM linkage function for the scheduler to return to the next process which is kernelland
// Inputs: None (not passed directly)
// Outputs: None (not passed directly)
// Side effects: Completes the scheduler servicing and IRETs to the next scheduled process in kernelmode
exit_sched_to_k:
    pushl %ebp
    movl %esp, %ebp
    
    subl $12, %esp // Allocate state values (3 longs) (CS, flags, esp)
    subl $SIZEOF_PUSHALL, %esp

    pushl %esp // Push struct to the state restoration as pointer
    call exit_sched_to_k_helper // exit_sched_to_k_helper(next_values)
    addl $4, %esp

    cmpl $-1, %eax
    jle _scheduling_fail

    POPALL_REGS_HW_CONTEXT
    # OK, so movw %eax, %cx is also illegal...
    # We have to inject a WHOLE goddamn IRET.
    popl %esp
    iret
    # popw %cs is illegal so here's the next best thing

_scheduling_fail:
    pushl %eax
    call schedule_failed
