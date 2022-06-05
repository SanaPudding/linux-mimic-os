#ifndef IDT_H
#define IDT_H

// We use ASM wrappers for everything because we want IRET for every exception handler, NOT normal return.
// Some take a return code, which we will have to discard ourselves. Otherwise our IRET might use the return code (could be 0!)
// See Table 5.1 in IA32 Manual Vol. 3.

// This generates labels/function names for the ASM wrappers for the IDT entries.
#define FORCE_EXPAND_IDT_ASM_WRAPPER(VECNUM) idt_asm_wrapper_ ## VECNUM
#define IDT_ASM_WRAPPER(VECNUM) FORCE_EXPAND_IDT_ASM_WRAPPER(VECNUM)

/* Defines for various vector numbers */
/* Vector numbers for exceptions */
#define IDT_DIVERR  0
#define IDT_INTEL_RESERVED 1
#define IDT_NMIINT  2
#define IDT_BREAK  3
#define IDT_OVERFLOW  4
#define IDT_BOUND  5
#define IDT_INVALOP  6
#define IDT_DEVICENA  7
#define IDT_DOUBLEFAULT  8
#define IDT_SEGMENT_OVERRUN_RESERVED 9
#define IDT_INVALTSS  10
#define IDT_SEGNOTPRESENT  11
#define IDT_STACKSEGFAULT  12
#define IDT_GENPROTECT  13
#define IDT_PAGEFAULT  14
#define IDT_INTEL_RESERVED_15 15
#define IDT_MATHFAULT  16
#define IDT_ALIGNCHK  17
#define IDT_MACHINECHK  18
#define IDT_SIMDFPE  19

#define IDT_EXCPT_END   19      // The last exception vector number

/* Vector numbers for interrupts */
#define IDT_KEYBOARD    0x21
#define IDT_RTC         0x28
#define IDT_PIT         0x20

#define IDT_INT_START   0x20    // The first interrupt vector number
#define IDT_INT_END     0x2F    // The last interrupt vector number

/* Vector numbers for system calls */
#define IDT_SYSCALL     0x80

// Very useful macros for making pre-made assembly wrappers for functions.
#define CREATE_RETCODE_EXCEPTION_WRAPPER(VECNUM) \
IDT_ASM_WRAPPER(VECNUM): \
    pushl $VECNUM; \
    jmp common_exception_asm;

#define CREATE_NORETCODE_EXCEPTION_WRAPPER(VECNUM) \
IDT_ASM_WRAPPER(VECNUM): \
    pushl $-2; \
    pushl $VECNUM; \
    jmp common_exception_asm;

#define CREATE_INTERRUPT_WRAPPER(NAME, VECNUM)   \
.globl NAME                                     ;\
NAME:                                           ;\
    pushl $DUMMY                                ;\
    pushl $VECNUM                               ;\
    jmp common_interrupt_asm;

#define HW_EBX 0 
#define HW_ECX 4
#define HW_EDX 8
#define HW_ESI 12
#define HW_EDI 16
#define HW_EBP 20
#define HW_EAX 24
#define HW_XDS 28
#define HW_XES 32
#define HW_VECNUM 36
#define HW_ERRCODE 40
#define HW_EIP 44
#define HW_XCS 48
#define HW_EFLAGS 52
#define HW_ESP 56
#define HW_XSS 60

#define PUSHALL_REGS_HW_CONTEXT \
    pushw $0; /* Little endian, %es comes first in memory, then the padding */ \
    pushw %es; \
    pushw $0; \
    pushw %ds; \
    pushl %eax; \
    pushl %ebp; \
    pushl %edi; \
    pushl %esi; \
    pushl %edx; \
    pushl %ecx; \
    pushl %ebx;

// Obtained by counting the number of bytes pushed above
#define SIZEOF_PUSHALL (8+7*4)

#define POPALL_REGS_HW_CONTEXT \
    popl %ebx; \
    popl %ecx; \
    popl %edx; \
    popl %esi; \
    popl %edi; \
    popl %ebp; \
    popl %eax; \
    popw %ds; \
    addl $2, %esp; \
    popw %es; \
    addl $2, %esp; \

#ifndef ASM
extern void idt_init();
#endif
#endif
