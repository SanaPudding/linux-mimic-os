#include "syscall.h"
#include "../x86_desc.h"
#include "sys_execute.h"
#include "../paging.h"
#include "parser.h"
#include "../process/process.h"
#include "../memfs/memfs.h"
#include "../device-drivers/keyboard.h"
#include "../lib.h"

// Function to let a user program execute a new child process
// Inputs: 
//      caller_context: context set up by the syscall handler in the IDT
//      kstack_context: new, synthesized context to fill with initial register/other state values
//          (also passes information about the EIP to return to, read helper functions)
// Outputs:
//      execute_result_t: Result struct describing if we were able to be fully initialized, or 
//                        if we failed and attained a partial initialization state and need to rollback
// Side effects: Launches a new process (changes tss0, paging, process control globals, and more...)
execute_result_t sys_execute_helper(
        hwcontext_t* caller_context,
        from_kernel_context_t* kstack_context
){
    // Set initial values
    execute_result_t rollback_info = {
        .retval = -1,
        .flag_allocated_proc = 0,
        .flag_configured_paging = 0,
        .flag_updated_esp0 = 0,
        .allocated_proc_id = FAIL_PID,
        .origin_proc_id = FAIL_PID
    };
    
    // Basic pointer checking
    if (!caller_context || !kstack_context) return rollback_info;
    
    // Set up kernel-style paging so we can access whatever memory we want
    set_new_cr3((uint32_t)kernel_page_descriptor_table);

    // Obtain the PCB PID values of the current PID (parent) and the next PID (child)
    pcb_t* this_pcb = get_current_pcb();
    if (!this_pcb) return rollback_info;
    uint32_t this_pid = this_pcb->pid;
    rollback_info.origin_proc_id = this_pid;

    pcb_t* next_pcb = process_allocate(this_pid);
    if (!next_pcb) return rollback_info;
    uint32_t next_pid = next_pcb->pid;
    rollback_info.flag_allocated_proc = 1;
    rollback_info.allocated_proc_id = next_pid;

    // Translate the command pointer from user to kernel, which does range checking (returns NULL if out of range so it passes all other checks)
    // NULL is the universal sign of invalidity - Confuscius 
    const char* input_cmd = (const char*)(
        translate_user_to_kernel((void*)(caller_context->ebx), this_pid)
    );
    
    // Parse the input command and the arguments using our parse_res struct
    parse_command_result_t parse_res = parse_command((const char*)input_cmd);
    if (parse_res.cmd_end_idx_excl == parse_res.cmd_start_idx_incl) return rollback_info;

    char progname[KEYBOARD_BUF_SIZE+1];
    if (extract_parsed_command(input_cmd, parse_res, progname, KEYBOARD_BUF_SIZE+1)) {
        return rollback_info;
    }

    // copy command into pcb argument field to read from later
    if (extract_parsed_args(input_cmd, parse_res, next_pcb->argument, KEYBOARD_BUF_SIZE+1)) {
        return rollback_info;
    }

    // Determine executability and load it all into memory
    executability_result_t exec_info = determine_executability(progname);
    if (!exec_info.is_executable) { return rollback_info; }

    if (create_new_user_programpage(next_pid) == -1) return rollback_info;

    activate_existing_user_programpage(next_pid);
    
    rollback_info.flag_configured_paging = 1;

    if (load_executable_into_memory(exec_info, next_pid) == -1) { return rollback_info; }
    
    // Save our hardware context before the interrupt in the parent, and initialize the 
    // kernel stack context so that we can enter the user process
    if (save_context_in_pcb(
            this_pcb,
            kstack_context,
            caller_context
        ) == -1) return rollback_info;

    uint32_t initial_user_esp = get_initial_esp_of_process(next_pid);
    uint32_t initial_user_eip = get_user_eip(exec_info);
    eflags_register_fmt_t inherited_flags = caller_context->iret_context.eflags;

    if (initialize_kstack_context(
        kstack_context,
        initial_user_eip,
        inherited_flags,
        initial_user_esp
    ) == -1) return rollback_info;

    // Set our TSS
    tss.esp0 = get_initial_esp0_of_process(next_pid);
    rollback_info.flag_updated_esp0 = 1;
    tss.ss0 = KERNEL_DS;


    // Mark as success and return
    rollback_info.retval = 0;
    return rollback_info;
}
