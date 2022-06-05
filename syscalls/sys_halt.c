#include "sys_halt.h"
#include "../paging.h"
#include "../x86_desc.h"
#include "../process/process.h"

int32_t fake_syshalt_for_roots(
    hwcontext_t* caller_context, 
    from_kernel_context_t* kstack_context, 
    uint32_t pid
);


// Helper to assist in running sys halt
// Inputs:
//      caller_context: Context of the caller who called syshalt (the dying program)
//      kstack_context: Kernel stack context that will be ripped off by IRET + POPAL after this helper function runs
// Outputs: 0 success, -1 failure
// Side effects: Halts the current process and deinitializes all associated variables, and/or decrements
int32_t sys_halt_helper(
    hwcontext_t* caller_context,
    from_kernel_context_t* kstack_context
) {
    
    set_new_cr3((uint32_t)kernel_page_descriptor_table);
    uint32_t status_code = (caller_context->ebx) & 0xFF; // Pass lower byte
    pcb_t* this_pcb = get_current_pcb();
    if (!this_pcb) return -1;
    // Get the parent
    pcb_t* next_pcb = get_pcb(this_pcb->parent_pid);
    if (!next_pcb) return -1;

    uint32_t this_pid = this_pcb->pid;
    uint32_t next_pid = next_pcb->pid;
    
    if (is_kernel_pid(next_pid)) {
        int retval = fake_syshalt_for_roots(caller_context, kstack_context, this_pid);
        return retval;
    }
    
    // Initialize parent reg/iret context to restore the stack we wanted to use, as well as registers
    // Configure PUSHA context of the parent's pusha context so that we can restore our state to the kernel's pusha
    kstack_context->pusha_context.ebx = next_pcb->pre_sysexec_kstack.pusha_context.ebx;
    kstack_context->pusha_context.ecx = next_pcb->pre_sysexec_kstack.pusha_context.ecx;
    kstack_context->pusha_context.edx = next_pcb->pre_sysexec_kstack.pusha_context.edx;
    kstack_context->pusha_context.edi = next_pcb->pre_sysexec_kstack.pusha_context.edi;
    kstack_context->pusha_context.esi = next_pcb->pre_sysexec_kstack.pusha_context.esi;
    kstack_context->pusha_context.ebp = next_pcb->pre_sysexec_kstack.pusha_context.ebp;

    // Update kstack_context's EAX with the return code value (to restore in the popal)
    kstack_context->pusha_context.eax = status_code;
    
    // This is the IRET we use to get back to the parent's kernel stack in execute, which will then
    // return from sys_execute and then to the parent process.
    kstack_context->iret_context.ret_eip = next_pcb->pre_sysexec_kstack.iret_context.ret_eip;
    kstack_context->iret_context.cs = KERNEL_CS;
    kstack_context->iret_context._pad_cs = 0;
    kstack_context->iret_context.eflags = next_pcb->pre_sysexec_kstack.iret_context.eflags;

    // Restore the data segment, uggghhhhh this was a bug, not initializing this
    kstack_context->ds = next_pcb->pre_sysexec_kstack.ds;
    kstack_context->_pad_ds = 0;

    // Note, these are don't cares because we're doing a Kernel to Kernel IRET ( no privilege switch )
    // The dummy values are magic numbers
    kstack_context->iret_context.esp = 0xFEEDBEEF;
    kstack_context->iret_context.ss = 0xCAFE;

    // Assumption: every time we privilege switch from a process to its 
    // OWN kernel stack, that kernel stack is empty
    // Consequence: every time we finish the original software-induced interrupt and switch from
    // a process's kernel stack to that same process's original stack, the kernel stack is once again empty
    // (note): restoring the original in the K-to-K IRET switch is not a privilege switch
    // If this assumption is wrong, then the esp0 value below is wrong
    tss.esp0 = get_initial_esp0_of_process(next_pid);
    tss.ss0 = KERNEL_DS;
    
    // Deactivate user memory
    if (next_pcb->flag_activated_vidmap) deactivate_user_vidmem();

    // Deallocate the PCB
    process_free(this_pid);

    // Offline the main memory
    destroy_user_programpage(this_pid);

    // Resetore the parent's PID
    activate_existing_user_programpage(next_pid);
    
    // If we are not returning to the root PID (kernel launch context), we should reenable the user paging system
    if (next_pid != 0) {
        set_new_cr3((uint32_t)user_page_descriptor_table);
    }

    return 0; 
}

// A fake syshalt to operate for root PIDs. This is because the return code of this process doesn't really matter, and it has to be restarted anyway, so we just reset the relevant user state
// That relevant user state is the general purpose registeres, ESP and EIP
// Inputs: 
//      caller_context: Hardware context from which sys halt was called (likely unused)
//      kstack_context: Context that will be used to perform the privilege switched
//      PID: the PID of the process that needs to be reset
// Outputs: 0 on success
// Side effects: Populates kstack_context for the IRET, and preps variables for the process to restart (also closes FDs of relevant processes)
int32_t fake_syshalt_for_roots(
    hwcontext_t* caller_context, 
    from_kernel_context_t* kstack_context, 
    uint32_t pid
) {
    uint32_t flags, garbage;
    CRITICAL_SECTION_FLAGSAVE(flags, garbage) {
        printf("Pid %d terminated.\n", pid);
        close_pid_fds(pid);
    }
    
    // Do not modify the user_page_descriptor_table
    pcb_t* this_pcb = get_pcb(pid);
    uint32_t reset_eip = get_user_eip(this_pcb->start_exec_info);
    uint32_t reset_esp = get_initial_esp_of_process(pid);
    
    kstack_context->pusha_context.ebx = 
        kstack_context->pusha_context.ecx =
        kstack_context->pusha_context.edx =
        kstack_context->pusha_context.edi =
        kstack_context->pusha_context.esi =
        kstack_context->pusha_context.eax =
        kstack_context->pusha_context.ebp = 0;

    kstack_context->iret_context.ret_eip = reset_eip;
    // Don't care about smashing the kernel stack, we're returning anyway.
    kstack_context->iret_context.cs = USER_CS;
    kstack_context->iret_context._pad_cs = 0;
    (void)kstack_context->iret_context.eflags; // Unmodified
    kstack_context->iret_context.esp = reset_esp;
    kstack_context->iret_context.ss = USER_DS;
    kstack_context->iret_context._pad_ss = 0;

    // Restore the data segment, uggghhhhh this was a bug, not initializing this
    kstack_context->ds = USER_DS;
    kstack_context->_pad_ds = 0;

    set_new_cr3((uint32_t)user_page_descriptor_table);

    // CLI for potentially problematic interrupts by the scheduler, this will be resolved in time with the IRET restarting EFLAGS
    cli();
    return 0;
}
