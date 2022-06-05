#include "syscall.h"
#include "parser.h"
#include "../device-drivers/keyboard.h" // KEYBOARD_BUF_SIZE
#include "../tests/tests.h"
#include "../memfs/memfs.h"
#include "../process/file.h"
#include "../process/process.h"
#include "../paging.h"
#include "../types.h"
#include "../lib.h"
#include "sys_execute.h"
#include "sys_halt.h"

// System execute in C (wrapped with ASM) to do most of the heavy lifting
// Inputs: 
//      caller_context: Hardware context of the process that made the system call
//      kstack_context: A kernel context to perform an IRET on (must be populated)
// Outputs: 0 on success, -1 on failure
// Side effects: Either executes a process, or does not - performs rollback operations on failure
int32_t sys_execute_c(hwcontext_t* caller_context, from_kernel_context_t* kstack_context){ 
    if (syscall_prologue()) return -1;
    execute_result_t rollback_info = sys_execute_helper(caller_context, kstack_context);
    int32_t retval = rollback_info.retval;
    if (retval == -1) {
        // Rollback in reverse order of initialization, for safety guarantees
        if (rollback_info.flag_updated_esp0) {
            tss.esp0 = get_initial_esp0_of_process(rollback_info.origin_proc_id);
        }
        if (rollback_info.flag_configured_paging) {
            destroy_user_programpage(rollback_info.allocated_proc_id);
        }
        if (rollback_info.flag_allocated_proc) {
            process_free(rollback_info.allocated_proc_id);
        }
    }
    if (syscall_epilogue()) return -1;
    return retval;
}

// System halt in C (wrapped with ASM) to do most of the heavy lifting
// Inputs: 
//      caller_context: Hardware context of the process that made the system call
//      kstack_context: A kernel context to perform an IRET on (must be populated)
// Outputs: 0 on success, -1 on failure
// Side effects: Halt a process, whatever "halting" a "process" means
int32_t sys_halt_c(hwcontext_t* caller_context, from_kernel_context_t* kstack_context) {
    if (syscall_prologue()) return -1;
    int32_t retval = sys_halt_helper(caller_context, kstack_context);
    if (syscall_epilogue()) return -1;
    return retval;
}


// System read in C (wrapped with ASM) to do most of the heavy lifting
// Inputs: 
//      caller_context: Hardware context of the process that made the system call
// Outputs: 0 on success, -1 on failure
// Side effects: Performs a system read according to user-passed arguments passed in EBX, ECX, EDX
int32_t sys_read(hwcontext_t* hw_context) {
    if (syscall_prologue()) return -1;
    // extract args from hw_context
    int32_t fd = (int32_t) hw_context->ebx;
    uint8_t* buf = (uint8_t*) hw_context->ecx;
    int32_t nbytes = (int32_t) hw_context->edx;

    // call generic_write
    int32_t retval = generic_read(fd, buf, nbytes);
    if (syscall_epilogue()) return -1;
    else return retval;
}

// System write in C (wrapped with ASM) to do most of the heavy lifting
// Inputs: 
//      caller_context: Hardware context of the process that made the system call
// Outputs: 0 on success, -1 on failure
// Side effects: Performs a system write according to user-passed arguments passed in EBX, ECX, EDX
int32_t sys_write(hwcontext_t* hw_context) {
    if (syscall_prologue()) return -1;
    // extract args from hw_context
    int32_t fd = (int32_t) hw_context->ebx;
    uint8_t* buf = (uint8_t*) hw_context->ecx;
    int32_t nbytes = (int32_t) hw_context->edx;

    // call generic_write
    int32_t retval = generic_write(fd, buf, nbytes);
    if (syscall_epilogue()) return -1;
    else return retval;
}

// System open in C (wrapped with ASM) to do most of the heavy lifting
// Inputs: 
//      caller_context: Hardware context of the process that made the system call
// Outputs: 0 on success, -1 on failure
// Side effects: Performs a system open according to user-passed arguments passed in EBX
int32_t sys_open(hwcontext_t* hw_context) {
    if (syscall_prologue()) return -1;
    // extract args from hw_context
    const uint8_t* filename = (const uint8_t*) hw_context->ebx;
    int32_t retval = generic_open(filename);
    if (syscall_epilogue()) return -1;
    return retval;
}


// System close in C (wrapped with ASM) to do most of the heavy lifting
// Inputs: 
//      caller_context: Hardware context of the process that made the system call
// Outputs: 0 on success, -1 on failure
// Side effects: Performs a system close according to user-passed arguments passed in EBX
int32_t sys_close(hwcontext_t* hw_context) {
    if (syscall_prologue()) return -1;
    // extract args from hw_context
    int32_t fd = (int32_t) hw_context->ebx;
    int32_t retval = generic_close(fd);
    if (syscall_epilogue()) return -1;
    return retval;
}

// Reads the program's command line arguments that is stored when the program is loaded into a user-level buffer
// 
// Inputs:
//      hw_context: hardware context
//      
// Output: 0 on success, -1 on failure
// Side effects: user-level buffer stores the current program's argument
int32_t sys_getargs(hwcontext_t* hw_context) {
    if(syscall_prologue()) return -1;
    // extract args from hw_context
    int32_t retval = 0;
    // get current process control block
    pcb_t* curr_pcb = get_current_pcb();
    uint8_t* k_buf = (uint8_t*) hw_context->ebx;
    uint8_t* buf = (uint8_t*)translate_user_to_kernel(k_buf, curr_pcb->pid);
    int32_t nbytes = (int32_t) hw_context->ecx;

    // check whether buf is a NULL pointer
    if (buf == NULL) retval = -1;
    else {
        // return -1 whether argument of the program exists and the length does not exceed nbytes
        if (strlen((char*)curr_pcb->argument) > nbytes || strlen((char*)curr_pcb->argument) == 0)
            retval = -1;
        else {
            // copy program's argument to buf
            strncpy((char*)buf, curr_pcb->argument, nbytes);
        }
    }
    if (syscall_epilogue()) return -1;
    else return retval;
}

// Sys vidmap, called from ASM 
// Inputs: the hardware context of the user
// Outputs: -1 on failure, 0 on success
// Side effects: performs the VIDMAP for the user
int32_t sys_vidmap(hwcontext_t* hw_context) {
    if (syscall_prologue()) return -1;
    int32_t retval = -1;
    do {
        pcb_t* curr = get_current_pcb();
        if (!curr) break;

        curr->flag_activated_vidmap = 1;

        uint8_t** buf_to_fill = translate_user_to_kernel((uint8_t**)hw_context->ebx, curr->pid);
        if (!buf_to_fill) break;

        *buf_to_fill = (uint8_t*) BEGINNING_USERVID_VIRTUAL_ADDR;
        if (activate_user_vidmem()) break;

        // Got to the bottom; success
        retval = 0;
    } while (0);
    if (syscall_epilogue()) return -1;
    return retval;
}

// Inputs:
//      hw_context: hardware context
//      
// Output: -1 (not implemented)
// Side effects: N/A
int32_t sys_set_handler(hwcontext_t* hw_context) {
    return -1;
}

// Inputs:
//      hw_context: hardware context
//      
// Output: -1 (not implemented)
// Side effects: N/A
int32_t sys_sigreturn(hwcontext_t* hw_context) {
    #ifdef SYSCALL_RET_TESTING
    return TEST_VALUE_xDEAD;
    #endif
    return -1;
}

// Performs all necessary operations for beginning the syscall (setting up kernel-side mapping)
int32_t syscall_prologue() {
    set_new_cr3((uint32_t)kernel_page_descriptor_table);
    return 0;
}

// Performs all necessary operations for ending the syscall (setting up user-side mapping)
int32_t syscall_epilogue() {
    set_new_cr3((uint32_t)user_page_descriptor_table);
    return 0;
}
