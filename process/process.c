#include "../x86_desc.h"
#include "../paging.h"
#include "../memfs/memfs.h"
#include "../lib.h"
#include "../device-drivers/terminal.h"
#include "process.h"
#include "../common.h"
#include "../sched/sched.h"

/* file-scope variables */
static uint32_t current_pid;
static int process_counter;
pcb_t root_pcb;

/* file-scope functions */
static uint32_t get_allocatable_pid();

// Translates a userspace address to an address for the kernel to use
// Inputs:
//      user_addr: Address to translate
//      nth_process: PID of the process
// Outputs: Address translated to kernelspace, NULL if the address is out-of-bounds by the user
void* translate_user_to_kernel(const void* user_addr, uint32_t pid) {
    uint32_t value = (uint32_t)user_addr;
    if (value >= BEGINNING_USERPAGE_VIRTUAL_ADDR && 
                value < BEGINNING_USERPAGE_VIRTUAL_ADDR + SIZEOF_PROGRAMPAGE) {
        uint32_t kern_value = value
                - BEGINNING_USERPAGE_VIRTUAL_ADDR
                + BEGINNING_USERPAGE_PHYSICAL_ADDR
                + (pid - 1) * SIZEOF_PROGRAMPAGE;
        return (void*)kern_value;
    } else {
        return 0;
    }
}

// Translates a kernel address to an address for a user with PID nth_process
// Inputs:
//      kern_addr: Address to translate
//      nth_process: PID of the process
// Outputs: Address translated to userspace, NULL if the address is out-of-bounds by the user
void* translate_kernel_to_user(const void* kern_addr, uint32_t pid) {
    uint32_t value = (uint32_t)kern_addr;
    const uint32_t USER_PROGMEM_PHYS_START = BEGINNING_USERPAGE_PHYSICAL_ADDR + (pid - 1) * SIZEOF_PROGRAMPAGE;
    if (value >= USER_PROGMEM_PHYS_START && value < USER_PROGMEM_PHYS_START + SIZEOF_PROGRAMPAGE) {
        uint32_t user_value = value
            - (pid - 1) * SIZEOF_PROGRAMPAGE
            - BEGINNING_USERPAGE_PHYSICAL_ADDR
            + BEGINNING_USERPAGE_VIRTUAL_ADDR;
        return (void*)user_value;
    } else {
        return 0;
    }
}

// Sets the only necessary parameters for the TSS
// Inputs: new_ss0, new_esp0
// Outputs: None
// Side effects: Loads the TSS with the new_ss0 and new_esp0 for privileged context switch
void update_tss_for_new_stack(uint16_t new_ss0, uint32_t new_esp0) {
    tss.ss0 = new_ss0;
    tss.esp0 = new_esp0;
}

// Returns the address for the kernel stack area for the process,
// which includes the address of the PCB
// Inputs: PID
// Outputs: Address of the process control area if the PID is valid, 0 else
// Side effects, none
proc_area_t* get_process_area_address(uint32_t pid) {
    // With (pid) - 0;
    //      PID=1's stack ends at 8MB
    // With (pid) - 1:
    //      PID=1's stack ends at 8MB - 8KB
    return (proc_area_t*)(BEGINNING_USERPAGE_PHYSICAL_ADDR) - (pid) - 1; // Add some padding 
}

proc_page_t* get_process_page_address(uint32_t pid) {
    return (proc_page_t*)(BEGINNING_USERPAGE_PHYSICAL_ADDR) + pid - 1;
}
// Loads the executable into memory for a specific process
// Inputs:
//      exec_info: Information about the executable
//      nth_process: PID of the process
// Outputs:
//      -1 if there was an error in loading to memory
//      0 otherwise
// Side effects:
//      Loads the executable described by the exec_info struct into the proper memory location
int32_t load_executable_into_memory(executability_result_t exec_info, uint32_t pid) {
    uint8_t* prog_image_target_addr = (uint8_t*)(
        translate_user_to_kernel((void*)TARGET_PROGRAM_LOCATION_VIRTUAL, pid)
    );

    if (!prog_image_target_addr) return -1;
    
    if (!exec_info.is_executable) return -1;

    if (exec_info.exec_file_length !=
        read_data(exec_info.exec_inode, 0, prog_image_target_addr, exec_info.exec_file_length)) {
            printf("Unable to copy to memory!\n");
            return -1;
        }

    // Sanity Check
    if (*(uint32_t*)(exec_info.start_eip) != *(uint32_t*)(prog_image_target_addr + EXEC_START_EIP_OFFSET)) {
        printf("Sanity check failed, extracted EIP and copied program image are not equal! \n");
        return -1;
    }
    return 0;
}

/*
 * process_init
 *     DESCRIPTION: Initialize process.c file scope variables, set all pcbs
 *                  as not present.
 *     INPUTS: none
 *     RETURN VALUE: none
 */
void process_init() {
    int i;

    // initialize file scope variables
    current_pid = 0; // Make true PIDs start at 1
    process_counter = 0;

    // set all user pcbs as inactivated
    for (i = 1; i <= MAX_NUM_PROCESS; i++) {
        pcb_t* curr_pcb = get_pcb(i);
        curr_pcb->pid = (uint32_t) i;
        curr_pcb->present = 0;
    }

    // Initialize the root PCB
    root_pcb.pid = 0;
    root_pcb.present = 1;
}

/*
 * process_allocate
 *     DESCRIPTION: Allocate and initialize an avaiable pcb if any.
 *     INPUTS: none
 *     RETURN VALUE: pointer to a pcb struct upon success, NULL upon failure.
 */
pcb_t* process_allocate(uint32_t parent) {
    // sanity checks
    if (process_counter > MAX_NUM_PROCESS) {return NULL;}

    // allocate a new pcb
    uint32_t new_pid = get_allocatable_pid();
    if (new_pid > MAX_NUM_PROCESS) {return NULL;}

    pcb_t* new_pcb;
    new_pcb = get_pcb(new_pid);
    if (!new_pcb) {return NULL;}

    // initialize the new pcb
    uint32_t flags, garbage;
    CRITICAL_SECTION_FLAGSAVE(flags, garbage) {
        initialize_fd_array(new_pcb->fd_array);
        new_pcb->present = 1;
        new_pcb->parent_pid = parent;
        new_pcb->flag_activated_vidmap = 0;
        process_counter++;
    }
    
    return new_pcb;
}

/*
 * process_free
 *     DESCRIPTION: Close/clear the pcb indexed by the input pid.
 *     INPUTS: pid -- process id.
 *     RETURN VALUE: 0 upon success, -1 upon failure.
 */
int process_free(uint32_t pid) {
    // sanity checks
    if (pid > MAX_NUM_PROCESS || process_counter == 0 || pid == 0) {return -1;}

    pcb_t* curr_pcb = get_pcb(pid);
    if (!curr_pcb) {return -1;}
    if (!curr_pcb->present) {return -1;}

    uint32_t parent = curr_pcb->parent_pid;
    // free the pcb of the current process
    uint32_t flags, garbage;
    CRITICAL_SECTION_FLAGSAVE(flags, garbage) {
        curr_pcb->present = 0;
        curr_pcb->flag_activated_vidmap = 0;
        close_pid_fds(pid);
        process_counter--;
    }

    return parent;
}

/*
 * get_current_pcb
 *     DESCRIPTION: Get the pointer to the current running process's pcb.
 *     INPUTS: none
 *     RETURN VALUE: pointer to the current running process's pcb if exists, NULL if not.
 */
pcb_t* get_current_pcb() {
    return get_pcb(get_current_pid());
}

/*
 * get_current_pid
 *     DESCRIPTION: Get the current process id.
 *     INPUTS: none
 *     RETURN VALUE: current pid
 */
uint32_t get_current_pid() {
    uint32_t curr_pid = derive_pid_from_esp();
    // PRINT_ASSERT(current_pid == exp, "current=%d, derived=%d", current_pid, exp);
    return curr_pid;
}

/*
 * get_allocatable_pid
 *     DESCRIPTION: Iterate through all reserved memory blocks for pcb, return an
 *                  index (pid) to an available block if any.
 *     INPUTS: none
 *     RETURN VALUE: valid pid upon success, FAIL_PID upon failure.
 */
static uint32_t get_allocatable_pid() {
    uint32_t ret_pid = FAIL_PID;
    uint32_t i;
    for (i = 0; i <= MAX_NUM_PROCESS; i++) {
        pcb_t* curr_pcb = get_pcb(i);
        if (!curr_pcb) {return FAIL_PID;}       // null check failed
        if (curr_pcb->present) {continue;}      // current pcb occupied
        ret_pid = i;
        break;
    }
    return ret_pid;
}

// Returns the address for the pcb for the process,
// Inputs: PID
// Outputs: Address of the process control block if the PID is valid, 0 else
// Side effects, none

pcb_t* get_pcb(uint32_t pid) {
    if (pid > MAX_NUM_PROCESS) {return NULL;}
    // The top of the process area holds the PCB, so we can just pointer cast this
    else if (pid) return (pcb_t*) (get_process_area_address(pid));
    else return &root_pcb; // PID = 0 means we are at the root PCB
}

// Saves the kernel state into the PCB.
// This is the function used for sys_execute and sys_halt, nothing to do with scheduler here.
// Inputs: pointer to PCB, location in stack to load the kernel information from, optionally a hardware context if necessary
// Outputs: 0 on success, -1 on failure.
// Side effects: Updates values in the PCB of interest
int32_t save_context_in_pcb(
        pcb_t* this_pcb, 
        const from_kernel_context_t* this_kstack_context, 
        const hwcontext_t* optional_this_hw_context
) {
    if (!this_pcb || !this_kstack_context) return -1;
    // Save some values that our kstack context gave us
    uint32_t post_execute_flags = this_kstack_context->iret_context.eflags.bits;
    uint32_t post_execute_reteip = this_kstack_context->iret_context.ret_eip;

    // Set kernel stack state to obvious settings (returning to kernel stack from halt)
    this_pcb->pre_sysexec_kstack.iret_context.ret_eip = post_execute_reteip;
    this_pcb->pre_sysexec_kstack.iret_context.cs = KERNEL_CS;
    this_pcb->pre_sysexec_kstack.iret_context.eflags.bits = post_execute_flags;

    // Save eax, ebx, ecx, edx, edi, esi, esp (for this stack) and ebp (for this stack)
    // Saving EBP is very important
    this_pcb->pre_sysexec_kstack.pusha_context = this_kstack_context->pusha_context;

    // Obvious setting of kernel DS but whatever, 
    this_pcb->pre_sysexec_kstack.ds = KERNEL_DS; 
    this_pcb->pre_sysexec_kstack._pad_ds = 0;
    
    // Copy the interrupt hardware context exactly 
    // (though honestly probably not necessary due to scheme of halt)
    if (optional_this_hw_context) {
        this_pcb->pre_sysexec_state = *optional_this_hw_context; 
    }
    return 0;
}

// Initializes the kernel stack context to do a switch to usermode
// This is used for sys_execute
// Inputs: Pointer to kstack_context to load, initial user values
// Outputs: 0 success, -1 failure
// Side effects: Populates kstack context for privilege switch to user
int32_t initialize_kstack_context(
        from_kernel_context_t* kstack_context, 
        uint32_t init_user_eip, 
        eflags_register_fmt_t init_user_flags, 
        uint32_t init_user_esp
) {
    if (!kstack_context) return -1;
    kstack_context->iret_context.ret_eip = init_user_eip;
    kstack_context->iret_context.cs = USER_CS;
    kstack_context->iret_context._pad_cs = 0;
    // Inherit flags from parent, I guess?
    kstack_context->iret_context.eflags = init_user_flags;
    kstack_context->iret_context.esp = init_user_esp;
    kstack_context->iret_context.ss = USER_DS;
    kstack_context->iret_context._pad_ss = 0;
    kstack_context->ds = USER_DS;
    kstack_context->_pad_ds = 0;
    // Initialize the GP registers (except ESP)
    kstack_context->pusha_context.eax =
        kstack_context->pusha_context.ebx =
        kstack_context->pusha_context.ecx =
        kstack_context->pusha_context.edx = 
        kstack_context->pusha_context.edi = 
        kstack_context->pusha_context.esi = 
        kstack_context->pusha_context.ebp = 0;
    
    return 0;
}

// Get the initial ESP0 of a process
// Used for sys_execute and prepping a new task, generally speaking
// Inputs: PID
// Outputs: Address to the stack position
// Notes: While other people use math directly in the form of macros, we use structs. Null checks are unnecessary, because we're not accessing memory.
uint32_t get_initial_esp0_of_process(uint32_t pid) {
    return (uint32_t)&(get_process_area_address(pid)->lowest_stack_elem);
}

// Get the initial ESP of a new user process
// Used for sys_execute and prepping a new task
// Inputs: PID of new process
// Outputs: ESP address of the process
// Notes: See the get_initial_esp0_of_process
uint32_t get_initial_esp_of_process(uint32_t pid) {
    return (uint32_t)(
        translate_kernel_to_user(&(get_process_page_address(pid)->lowest_user_stack_elem), pid)
    );
}

// Function to return whether the PID represents the kernel
// Added as a helper function because it was unclear if we would use this heavily, or if the requirement would change
// Inputs: PID
// Outputs: Truthy value representing whether the PID represents the initial kernel
int is_kernel_pid(uint32_t pid) {
    return pid == 0;
}

// Function to return whether the PID is a root process
// Necessary for scheduling and sys_halt, as these are the initial processes and the halt sequence is different
// Inputs: PID
// Outputs: Truthy value representing whether the PID represents a root process
int is_root_pid(uint32_t pid) {
    return pid <= NUM_SIMULTANEOUS_PROCS && pid != 0;
}


// Inputs: A number that is like an ESP address
// Outputs: PID mathematically derived from the ESP address
uint32_t derive_pid_from_esplike(uint32_t num) {
    const uint32_t nth_bit_set = 13; // Calculated with the hex value representation of 8KB
    uint32_t offset_start = (KERN_BEGIN_ADDR + SIZEOF_PROGRAMPAGE - 1) >> nth_bit_set;
    return offset_start - (num >> nth_bit_set);
}

// Linux does something like this, we can do something like this too!
// This helps us to worry less about setting a current_pid global
// Inputs: None
// Outputs: PID as the ESP tells us (tells us whose process kernel stack we're on)
uint32_t derive_pid_from_esp() {
    uint32_t espval;
    asm (
        "movl %%esp, %[pid]"
        : [pid] "=m" (espval)
        :
    );
    return derive_pid_from_esplike(espval);
}

// Derive the PID from the tss.esp0 value of a process
// Was made for potential future use but never saw service
// Inputs: None
// Outputs: PID as the tss tell sus
int32_t derive_pid_from_tss() {
    return derive_pid_from_esplike(tss.esp0);
}

// Identify the ultimate parent of a given PID
// Often used to identify the root PID, and subsequently an index into an array of possibly virtualized devices
// Inputs: PID
// Outputs: the PID of its ultimate parent.
uint32_t get_canonical_pid(uint32_t pid) {
    pcb_t* pcb = get_pcb(pid);
    if (!pcb) return FAIL_PID;
    uint32_t curr = pcb->pid;
    while (get_pcb(curr)->parent_pid) {
        curr = get_pcb(curr)->parent_pid;
    }
    return curr;
}

// Close all file descriptors of a given PID
// Inputs: PID
// Outputs: None
// Side effects: All open FDs would be closed
void close_pid_fds(uint32_t pid) {
    pcb_t* curr_pcb = get_pcb(pid);
    PRINT_ASSERT(curr_pcb != NULL, "Cannot close the FDs of PID %d!\n", pid);
    int i;
    // Ignore STDOUT and STDIN
    for (i = STDOUT_FD + 1; i < MAX_NUM_FD; i++) {
        if (curr_pcb->fd_array[i].present) {
            curr_pcb->fd_array[i].operations->close();
        }
    }
}
