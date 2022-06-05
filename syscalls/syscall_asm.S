.globl sys_execute
.globl sys_execute_c
.globl sys_halt
.globl sys_halt_c
.globl entrypoint_launch_from_kernel
.globl putc

// Just a forward declaration
.globl removes_eip_from_stack

#define ONE_LONG 4


#define ALLOCATE_MANUAL_REGS \
    subl $ONE_LONG, %esp; /* DS, stored as [S, D, 0, 0] from [top, ...., bottom] */ \
    subl $ONE_LONG, %esp; /* EBP */ \
    subl $ONE_LONG, %esp; /* ESI */ \
    subl $ONE_LONG, %esp; /* EDI */ \
    subl $ONE_LONG, %esp; /* EDX */ \
    subl $ONE_LONG, %esp; /* ECX */ \
    subl $ONE_LONG, %esp; /* EBX */ \
    subl $ONE_LONG, %esp; /* EAX */ \

#define POP_INTO_MANUAL_REGS \
    popl %eax; \
    popl %ebx; \
    popl %ecx; \
    popl %edx; \
    popl %edi; \
    popl %esi; \
    popl %ebp; \
    popw %ds; \
    addl $2, %esp; \

#define PUSH_KERNEL_CONTEXT(where_to_return) \
    /* IRET Context */ \
    subl $ONE_LONG, %esp; /* ss_user */ \
    subl $ONE_LONG, %esp; /* esp_user */ \
    pushfl; /* eflags */ \
    subl $ONE_LONG, %esp; /* cs + padding */ \
    lea where_to_return, %eax; \
    pushl %eax; /* ret EIP */ \
    /* Data segment selector + padding */ \
    subl $ONE_LONG, %esp; \
    /* GP Registers */ \
    pushal;

#define POP_INTO_KERNEL_CONTEXT \
    popal; \
    /* Fill data segment register */ \
    popw %ds; /* Little endian, important bits first */ \
    addl $2, %esp; /* Discard zero padding */ \
    /* Fill IRET context registers (ss_user, esp_user, cs, eflags, ret_eip) */ \
    iret;

#define TRASH_KERNEL_CONTEXT \
    addl $ONE_LONG, %esp; /* edi */ \
    addl $ONE_LONG, %esp; /* esi */ \
    addl $ONE_LONG, %esp; /* ebp */ \
    addl $ONE_LONG, %esp; /* esp */ \
    addl $ONE_LONG, %esp; /* ebx */ \
    addl $ONE_LONG, %esp; /* edx */ \
    addl $ONE_LONG, %esp; /* ecx */ \
    addl $ONE_LONG, %esp; /* eax */ \
    addl $ONE_LONG, %esp; /* ds+padding */ \
    addl $ONE_LONG, %esp; /* iret-eip */ \
    addl $ONE_LONG, %esp; /* iret-cs+pad */ \
    addl $ONE_LONG, %esp; /* iret-flags */ \
    addl $ONE_LONG, %esp; /* iret-esp */ \
    addl $ONE_LONG, %esp; /* iret-ss+pad */ \

sys_execute:
    pushl %ebp;
    movl %esp, %ebp;
    
    // Note: Because we're pushing this instruction (which points to where the stack frame is being ripped off),
    // even if %ESP is pointing to another stack, movl %ebp, %esp will return us to the proper stack
    // as long as EBP is correct! (It would be because we're pushing EBP with pushal)
    PUSH_KERNEL_CONTEXT(end_of_sys_execute)

    pushl %esp // Pointer to kernel context
    pushl 8(%ebp); // Pointer to caller hardware context
    // Fact: this sys_execute is only called from a user, so all IRET stack items will always be used
    call sys_execute_c;
    addl $8, %esp
    cmpl $0, %eax
    jne end_of_sys_execute;
    POP_INTO_KERNEL_CONTEXT

end_of_sys_execute:
    // movl %ebp, %esp will restore the state of %esp so it doesn't matter if %esp
    // is still pointing to a different stack
    movl %ebp, %esp
    popl %ebp
    ret;

sys_halt:
    pushl %ebp
    movl %esp, %ebp
    PUSH_KERNEL_CONTEXT(dummy_addr) // The symbol in here is arbitrary, we are going to modify this
    pushl %esp // Push pointer to kernel context
    pushl 8(%ebp) // Pointer to caller context, to access the status code
    call sys_halt_c
    addl $8, %esp
    cmpl $-1, %eax
    je busy_wait
    POP_INTO_KERNEL_CONTEXT

// // Inputs (on stack): pointer to the command to run, process ID to propogate
// entrypoint_launch_from_kernel:
//     pushl %ebp
//     movl %esp, %ebp

//     PUSH_KERNEL_CONTEXT(end_of_entrypoint_launch_from_kernel)

//     // Prepare the calling of c helper
//     pushl %esp // Pointer to launching struct
//     pushl 8(%ebp) // pointer to command string
//     call do_launch_from_kernel

//     // Discard stack arguments
//     addl $8, %esp
//     cmpl $-1, %eax
//     je busy_wait
    
//     POP_INTO_KERNEL_CONTEXT

// // We get here from halt
// end_of_entrypoint_launch_from_kernel:
//     movl %ebp, %esp
//     // Now we're back to the caller kernel stack!
//     popl %ebp
//     ret

// Constantly print X to the screen if we failed
busy_wait:  
    pushl $88 // character for X
    call putc
    addl $4, %esp
    jmp busy_wait

dummy_addr:
    jmp dummy_addr;
