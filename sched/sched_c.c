#include "../lib.h"
#include "sched.h"
#include "../process/process.h"
#include "../paging.h"
#include "../device-drivers/pit.h"
#include "../device-drivers/terminal.h"

static int pids_for_states[NUM_SIMULTANEOUS_PROCS];
static int pfs_ptr;
static int ignore_prior_state_for_init_flag;

int prep_shell_task(uint32_t pid);
uint32_t* get_prekint_esp(uint32_t* post_int_esp);
int load_resuming_state_user(exit_sched_to_u_context_t* destination, uint32_t resume_pid);
int load_resuming_state_kernel(exit_sched_to_k_context_t* destination, uint32_t resume_pid);
int store_universal_state_in_pcb(sched_hwcontext_t* proc_context);
void exit_sched_to_u();
void exit_sched_to_k();

// Initializes all necessary scheduler values (note: does NOT touch the PIT)
// Inputs: None
// Outputs: None
// Side effects: Allocates PIDs, initializes the pids_for_states array, preps the PIDs as if they were all preempted on initial execution
int sched_init() {
    int i;
    for (i = 0; i < NUM_SIMULTANEOUS_PROCS; i++) {
        pcb_t* root_pcb = process_allocate(NO_PARENT_PID);
        if (!root_pcb) return -1;
        uint32_t root_pid = root_pcb->pid;
        pids_for_states[i] = root_pid;
        if (prep_shell_task(root_pid)) return -1;
    }
    // For the first sched hit, we do not load any initial state
    ignore_prior_state_for_init_flag = 1;
    // Start at 0th entry in pids_for_states
    // (This marks the 2nd procstream as in use, leaving the 0th to be launched.)
    pfs_ptr = NUM_SIMULTANEOUS_PROCS - 1;
    return 0;
}

// Marks the current PID for future reloading and gets the next scheduled PID
// Inputs: PID to store
// Outputs: PID that will be resumed
// Side effects: Updates the "current" scheduled PID that has to be resumed later, marks the next PID as scheduled
int set_current_and_get_next_scheduled_pid(uint32_t pid) {
    pids_for_states[pfs_ptr] = pid;
    pfs_ptr = (pfs_ptr + 1) % NUM_SIMULTANEOUS_PROCS;
    return pids_for_states[pfs_ptr];
}

// Look ahead at the next scheduled PID without updating any state
// Inputs: None
// Outputs: The next scheduled PID
int peek_next_scheduled_pid() {
    return pids_for_states[(pfs_ptr + 1) % NUM_SIMULTANEOUS_PROCS];
}

// Injects a kernel IRET context into the PCB for a PID that will be returned to in kernel mode
// This is necessary because we cannot modify ESP and EIP in one go 
//      1. Modifying ESP means the stack is different, where does EIP come from? 
//      2. Modifying EIP means the instruction is different, when do we pop ESP?
// Inputs: target_pid, the PID whose PCB we will modify
// Outputs: 0 on success, -1 on failure
// Side effects: 
//      1. Modifies the ESP in the PCB to be decremented (more values injected)
//      2. Modifies the memory at ESP to have the IRET context implied by the PCB values
int inject_kiret_into_kstack(uint32_t target_pid) {
    pcb_t* target_pcb = get_pcb(target_pid);
    if (!target_pcb) return -1;

    uint32_t* new_esp = (uint32_t*)(target_pcb->universal_state.iret_regs.esp) - 3;
    new_esp[0] = target_pcb->universal_state.iret_regs.ret_eip;
    new_esp[1] = target_pcb->universal_state.iret_regs.cs;
    new_esp[2] = target_pcb->universal_state.iret_regs.eflags.bits;

    target_pcb->universal_state.iret_regs.esp = (uint32_t)new_esp;
    return 0;
}

// Gets the PID to load state from -- aka, the next scheduled PID
// Why is it called loadfrom? All values in pids_for_states are consciously set by us; they will not
// change under our noses, so they will always have valid info.
// Inputs: none
// Outputs: the loadfrom PID
// Side effects: Really?
int get_loadfrom_pid() {
    return pids_for_states[pfs_ptr];
}

// Gets the PID to store state to -- aka, the preempted PID
// Why is it called storeto? Because the PID may change if we end up halting or executing, so this
// would be where we would store our current state instead of what we last recorded.
// This may change during the quanta(??? sorry IDK vocabulary, I only know CPU go brrrrrrrrrrrrrrr)
// Inputs: none
// Outputs: the storeto PID
// Side effects: Returns the PID from which state must be stored into
int get_storeto_pid() {
    if (ignore_prior_state_for_init_flag) return NUM_SIMULTANEOUS_PROCS;
    return derive_pid_from_esp();
}

// Function to notify that scheduling failed
// Inputs: retval
// Outputs: None
// Side effects: Busy loop and print to screen
void schedule_failed(int retval) {
    printf("Schedule failed with error code %d!\n", retval);
    while (1) {}
}

// Called from an ASM function, do complex loading in C for return to the process in kernel mode
// Inputs: fill_context: The context to fill all state for the scheduler to resume
// Outputs: None
// Side effects: Updates fill_context such that on return of this function and returning to ASM linkage, the return to the process's kernel mode works
void exit_sched_to_k_helper(exit_sched_to_k_context_t* fill_context) {
    uint32_t preempted_pid = get_storeto_pid();
    uint32_t next_pid = set_current_and_get_next_scheduled_pid(preempted_pid);
    load_resuming_state_kernel(fill_context, next_pid);
    ignore_prior_state_for_init_flag = 0;
}

// Called from an ASM function, do complex loading in C for return to the process in user mode
// Inputs: fill_context: The context to fill all state for the scheduler to resume
// Outputs: None
// Side effects: Updates fill_context such that on return of this function and returning to ASM linkage, the return to the process's user mode works
void exit_sched_to_u_helper(exit_sched_to_u_context_t* fill_context) {
    uint32_t preempted_pid = get_storeto_pid();
    uint32_t next_pid = set_current_and_get_next_scheduled_pid(preempted_pid);
    load_resuming_state_user(fill_context, next_pid);
    ignore_prior_state_for_init_flag = 0;
}

// Handles the PIT interrupt, called through the PIT asm linkage
// Inputs: The hardware context as was initialized by the IDT function
// Outputs: 0 on success
// Side effects: Completes a scheduling cycle
int32_t handle_pit_interrupt(sched_hwcontext_t* proc_context) {
    if (!ignore_prior_state_for_init_flag) {
        store_universal_state_in_pcb(proc_context);
    }
    send_eoi(PIT_IRQ);
    uint32_t next_pid = peek_next_scheduled_pid();

    set_active_terminal(get_canonical_pid(next_pid) - 1);

    pcb_t* next_pcb = get_pcb(next_pid);
    if (next_pcb->universal_state.iret_regs.cs == KERNEL_CS) {
        exit_sched_to_k();
    } else if (next_pcb->universal_state.iret_regs.cs == USER_CS) {
        exit_sched_to_u();
    } else {
        PRINT_ASSERT(0, "Bad saved CS value!!\n");
    }
    return 0;
}

// Prepares the PCB and memroy for the PID to initialize at a program, very helpful for testing scheduling without the terminals
// Inputs: PID to prep
// Outputs: 0 on success, -1 on failure
// Side effects: Initializes the PCB and memory for a process to begin execution
int prep_shell_task(uint32_t pid) {
    #define INIT_PROGRAM "shell"

    pcb_t* the_pcb = get_pcb(pid);
    if (!the_pcb) return -1;
    
    // Parse the command and see if we *have* a command to parse (by checking the length)
    parse_command_result_t parse_res = parse_command((const char*)INIT_PROGRAM);
    // Extract the program name with our parsing info
    char progname[KEYBOARD_BUF_SIZE+1];
    extract_parsed_command(INIT_PROGRAM, parse_res, progname, KEYBOARD_BUF_SIZE+1);
    extract_parsed_args(INIT_PROGRAM, parse_res, the_pcb->argument, KEYBOARD_BUF_SIZE+1);
    executability_result_t exec_info = determine_executability(progname);
    the_pcb->start_exec_info = exec_info;

    // Initialize the very first program page and load the executable into the memory
    create_new_user_programpage(pid);
    activate_existing_user_programpage(pid);
    load_executable_into_memory(exec_info,pid);

    // Set up the first user ESP and EIP using structs for lazy addressing :^)
    // Math? Who wants to do MATH when the compiler can do it for us!?!?
    uint32_t init_user_eip = get_user_eip(exec_info);
    uint32_t init_user_esp = get_initial_esp_of_process(pid);
    
    eflags_register_fmt_t inherited_flags = get_eflags();
    inherited_flags.int_f = 1; // Interrupts must happen in the user program.
    
    the_pcb->universal_state.esp0 = get_initial_esp0_of_process(pid);
    the_pcb->universal_state.paging_state = init_root_proc_paging_state(pid);
    the_pcb->universal_state.gp_regs.eax
        = the_pcb->universal_state.gp_regs.ebx
        = the_pcb->universal_state.gp_regs.ecx
        = the_pcb->universal_state.gp_regs.edx
        = the_pcb->universal_state.gp_regs.edi
        = the_pcb->universal_state.gp_regs.esi
        = the_pcb->universal_state.gp_regs.ebp = 0;
    
    the_pcb->universal_state.gp_regs.ds = USER_DS;
    the_pcb->universal_state.gp_regs._pad_ds = 0;
    the_pcb->universal_state.gp_regs.es = 0;
    the_pcb->universal_state.gp_regs._pad_es = 0;

    the_pcb->universal_state.iret_regs.esp = init_user_esp;
    the_pcb->universal_state.iret_regs.ret_eip = init_user_eip;
    the_pcb->universal_state.iret_regs.eflags = inherited_flags;
    the_pcb->universal_state.iret_regs.cs = USER_CS;
    the_pcb->universal_state.iret_regs._pad_cs = 0;
    the_pcb->universal_state.iret_regs.ss = USER_DS;
    
    return 0;
}

// Calculate the ESP before the kernel-mode interrupt occurred
// Important for restoring original state
// Inputs: The post_interrupt ESP as a pointer
// Outputs: The pre-interrupt ESP as a pointer
// Side effects: None, it's just math
uint32_t* get_prekint_esp(uint32_t* post_int_esp) {
    const uint32_t NUM_KIRET_LONGS = 3;
    return post_int_esp + NUM_KIRET_LONGS;
}

// Store the state of the universe in the PCB
// The universe represents the state of the computer that the process is exposed to, which is potentially subject to change during a scheduling cycle
// Inputs: Scheduler context of the interrupted context
// Outputs: 0 on success, -1 on failure
// Side effects: Stores the state described by proc_context in the PCB
int store_universal_state_in_pcb(sched_hwcontext_t* proc_context) {
    uint32_t store_pid = get_storeto_pid();
    pcb_t* store_pcb = get_pcb(store_pid);
    
    if (!store_pcb) return -1;

    store_pcb->universal_state.gp_regs = proc_context->regs_context;
    store_pcb->universal_state.iret_regs._pad_cs = 0;
    store_pcb->universal_state.iret_regs.eflags = proc_context->iret_context.eflags;

    store_pcb->universal_state.esp0 = tss.esp0;
    store_pcb->universal_state.paging_state = current_universe_paging_state();

    if (proc_context->iret_context.cs == USER_CS) {
        store_pcb->universal_state.iret_regs.cs = USER_CS;
        store_pcb->universal_state.iret_regs.ss = USER_DS;
        store_pcb->universal_state.iret_regs._pad_ss = 0;
        store_pcb->universal_state.iret_regs.esp = proc_context->iret_context.esp;
        store_pcb->universal_state.iret_regs.ret_eip = proc_context->iret_context.ret_eip;
    } else if (proc_context->iret_context.cs == KERNEL_CS) {
        store_pcb->universal_state.iret_regs.cs = KERNEL_CS;
        store_pcb->universal_state.iret_regs.ss = KERNEL_DS;
        store_pcb->universal_state.iret_regs._pad_ss = 0;
        store_pcb->universal_state.iret_regs.esp = (uint32_t)get_prekint_esp(proc_context->post_int_esp);
        store_pcb->universal_state.iret_regs.ret_eip = proc_context->iret_context.ret_eip;
        inject_kiret_into_kstack(store_pid);
    } else {
        PRINT_ASSERT(0, "Malformed CS in PIT interrupt!");
    }

    return 0;
}

// Loads the resuming state of a process in usermode
// Inputs: Exit hardware context to load this new state into the process on exit (not ON on, but eventually on exit), PID of the process to be resumed
// Outputs: 0 on success, -1 on failure
// Side effects: Loads the state necessary into the destination context, loads other, non-stack/register related computer state (like paging)
int load_resuming_state_user(exit_sched_to_u_context_t* destination, uint32_t resume_pid) {
    PRINT_ASSERT(destination != 0, "Cannot load universe into a null destination (u)!\n");
    pcb_t* source_pcb = get_pcb(resume_pid);
    if (!source_pcb) return -1;
    destination->regs_context = source_pcb->universal_state.gp_regs;
    destination->iret_context = source_pcb->universal_state.iret_regs;
    tss.ss0 = KERNEL_DS;
    tss.esp0 = source_pcb->universal_state.esp0;
    load_paging_state_to_universe(source_pcb->universal_state.paging_state);
    return 0;
}

// Loads the resuming state of a process in kernelmode
// Same as load_resuming_state_user, but for kernel mode
// Inputs: Destination hardware context as a pointer, PID of the process to be resumed
// Outputs: 0 success, -1 failure
// Side effects: Same side effects as load_resuming_state_user, but for returning to kernel mode
int load_resuming_state_kernel(exit_sched_to_k_context_t* destination, uint32_t resume_pid) {
    PRINT_ASSERT(destination != 0, "Cannot load universe into a null destination (k)!\n");
    pcb_t* source_pcb = get_pcb(resume_pid);
    if (!source_pcb) return -1;
    destination->regs_context = source_pcb->universal_state.gp_regs;

    // Assumption: the PCB already has an injected RETEIP, flags, and CS into the destination stack,
    // so it will not be reinitialized here.
    destination->next_esp = (uint32_t*)source_pcb->universal_state.iret_regs.esp;

    tss.ss0 = KERNEL_DS;
    tss.esp0 = source_pcb->universal_state.esp0;
    load_paging_state_to_universe(source_pcb->universal_state.paging_state);
    return 0;
}
