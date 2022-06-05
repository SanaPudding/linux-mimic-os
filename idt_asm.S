#define ASM 1
#include "x86_desc.h"
#include "idt.h"

.globl IDT_ASM_WRAPPER(IDT_DIVERR)
.globl IDT_ASM_WRAPPER(IDT_INTEL_RESERVED)
.globl IDT_ASM_WRAPPER(IDT_NMIINT)
.globl IDT_ASM_WRAPPER(IDT_BREAK)
.globl IDT_ASM_WRAPPER(IDT_OVERFLOW)
.globl IDT_ASM_WRAPPER(IDT_BOUND)
.globl IDT_ASM_WRAPPER(IDT_INVALOP)
.globl IDT_ASM_WRAPPER(IDT_DEVICENA)
.globl IDT_ASM_WRAPPER(IDT_DOUBLEFAULT)
.globl IDT_ASM_WRAPPER(IDT_SEGMENT_OVERRUN_RESERVED)
.globl IDT_ASM_WRAPPER(IDT_INVALTSS)
.globl IDT_ASM_WRAPPER(IDT_SEGNOTPRESENT)
.globl IDT_ASM_WRAPPER(IDT_STACKSEGFAULT)
.globl IDT_ASM_WRAPPER(IDT_GENPROTECT)
.globl IDT_ASM_WRAPPER(IDT_PAGEFAULT)
.globl IDT_ASM_WRAPPER(IDT_INTEL_RESERVED_15)
.globl IDT_ASM_WRAPPER(IDT_MATHFAULT)
.globl IDT_ASM_WRAPPER(IDT_ALIGNCHK)
.globl IDT_ASM_WRAPPER(IDT_MACHINECHK)
.globl IDT_ASM_WRAPPER(IDT_SIMDFPE)

.globl idt_asm_wrapper_syscall

.globl common_exception_handler
.globl common_interrupt_handler

CREATE_NORETCODE_EXCEPTION_WRAPPER(IDT_DIVERR);
CREATE_NORETCODE_EXCEPTION_WRAPPER(IDT_INTEL_RESERVED);
CREATE_NORETCODE_EXCEPTION_WRAPPER(IDT_NMIINT);
CREATE_NORETCODE_EXCEPTION_WRAPPER(IDT_BREAK);
CREATE_NORETCODE_EXCEPTION_WRAPPER(IDT_OVERFLOW);
CREATE_NORETCODE_EXCEPTION_WRAPPER(IDT_BOUND);
CREATE_NORETCODE_EXCEPTION_WRAPPER(IDT_INVALOP);
CREATE_NORETCODE_EXCEPTION_WRAPPER(IDT_DEVICENA);
CREATE_RETCODE_EXCEPTION_WRAPPER(IDT_DOUBLEFAULT);
CREATE_NORETCODE_EXCEPTION_WRAPPER(IDT_SEGMENT_OVERRUN_RESERVED);
CREATE_RETCODE_EXCEPTION_WRAPPER(IDT_INVALTSS);
CREATE_RETCODE_EXCEPTION_WRAPPER(IDT_SEGNOTPRESENT);
CREATE_RETCODE_EXCEPTION_WRAPPER(IDT_STACKSEGFAULT);
CREATE_RETCODE_EXCEPTION_WRAPPER(IDT_GENPROTECT);
CREATE_RETCODE_EXCEPTION_WRAPPER(IDT_PAGEFAULT);
CREATE_NORETCODE_EXCEPTION_WRAPPER(IDT_INTEL_RESERVED_15);
CREATE_NORETCODE_EXCEPTION_WRAPPER(IDT_MATHFAULT);
CREATE_RETCODE_EXCEPTION_WRAPPER(IDT_ALIGNCHK);
CREATE_NORETCODE_EXCEPTION_WRAPPER(IDT_MACHINECHK);
CREATE_NORETCODE_EXCEPTION_WRAPPER(IDT_SIMDFPE);

.data
    NUM_SYSCALLS = 10
    DUMMY = 0xECEB

CREATE_INTERRUPT_WRAPPER(keyboard_interrupt_wrapper, IDT_KEYBOARD);
CREATE_INTERRUPT_WRAPPER(rtc_interrupt_wrapper, IDT_RTC);

common_interrupt_asm:
    PUSHALL_REGS_HW_CONTEXT
    pushl %esp      // Pass pointer to context
    call common_interrupt_handler
    addl $4, %esp   // Discard pointer to context
    POPALL_REGS_HW_CONTEXT
    addl $8, %esp
    iret

common_exception_asm:
    PUSHALL_REGS_HW_CONTEXT
    pushl %esp      // Pass pointer to context
    call common_exception_handler
    addl $4, %esp   // Discard pointer to context
    POPALL_REGS_HW_CONTEXT
    addl $8, %esp 
    iret

 
// System calls start at 0x1, 0x0 is not a valid system call!
.globl sys_halt, sys_execute, sys_read, sys_write, sys_open, sys_close, sys_getargs, sys_vidmap, sys_set_handler, sys_sigreturn
syscall_functions:
    .long 0x0, sys_halt, sys_execute, sys_read, sys_write, sys_open, sys_close, sys_getargs, sys_vidmap, sys_set_handler, sys_sigreturn

idt_asm_wrapper_syscall:
    pushl $DUMMY
    pushl $IDT_SYSCALL
    cmpl $1, %eax
    jl _bad_syscall
    cmpl $NUM_SYSCALLS, %eax
    jg _bad_syscall

    PUSHALL_REGS_HW_CONTEXT
    pushl %esp      // Pass pointer to HW struct as argument
    call *syscall_functions(,%eax, 4)
    addl $4, %esp   // Remove pointer from HW struct
    movl %eax, HW_EAX(%esp) // Load %eax value into the context struct
    POPALL_REGS_HW_CONTEXT
    addl $8, %esp   // Discard vector number and [dummy] error code
    iret

_bad_syscall:
    movl $-1, %eax
    addl $8, %esp // Discard vector number and [dummy] error code
    iret
