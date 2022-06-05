#ifndef PROCESS_H
#define PROCESS_H

#include "../x86_desc.h"
#include "../common.h"
#include "../syscalls/parser.h"
#include "../syscalls/syscall.h"
#include "../paging.h"
#include "file.h"
#include "../device-drivers/keyboard.h"

#define FAIL_PID        ((uint32_t)-1)
#define MAX_NUM_PROCESS 6

#define KERNEL_START_ADDR   0x400000
#define KERNEL_END_ADDR     0x800000
#define PROC_AREA_SIZE (8 * ONE_KB)

typedef struct universal_state_t {
    regs_hwcontext_t gp_regs;
    iret_context_t iret_regs;
    proc_paging_state_t paging_state;
    uint32_t esp0;
} universal_state_t;

typedef struct pcb_t {
    universal_state_t universal_state;
    parse_command_result_t create_command_info;
    executability_result_t start_exec_info;
    hwcontext_t pre_sysexec_state;
    from_kernel_context_t pre_sysexec_kstack;
    file_descriptor_t fd_array[MAX_NUM_FD];
    uint32_t pid;
    uint32_t parent_pid;
    uint32_t present;
    char argument[KEYBOARD_BUF_SIZE+1];
    uint32_t flag_activated_vidmap;
} pcb_t;

extern pcb_t root_pcb;

// Never allocate this on the stack
typedef struct pcb_page {
    uint8_t data[4 * ONE_MB - 2*sizeof(uint32_t)];
    uint32_t lowest_user_stack_elem;
    uint32_t _pad_to_avoid_pagefault; // I think the user will be fine with four bytes less
} __attribute__((packed)) proc_page_t;
STATIC_ASSERT(sizeof(proc_page_t) == 4*ONE_MB);

#define PROC_AREA_SIZE (8 * ONE_KB)
#define NO_PARENT_PID 0
// Struct defined for easier pointer arithmetic
// Don't allocate on the stack, just use it for pointer arithmetic
typedef struct proc_area_t {
    pcb_t pcb;
    uint8_t kstack_values[PROC_AREA_SIZE - sizeof(pcb_t) - sizeof(uint32_t)];
    uint32_t lowest_stack_elem;
} __attribute__((packed)) proc_area_t;
STATIC_ASSERT(sizeof(proc_area_t) == PROC_AREA_SIZE);

uint32_t get_initial_esp0_of_process(uint32_t pid);
uint32_t get_initial_esp_of_process(uint32_t pid);
void update_tss_for_new_stack(uint16_t new_ss0, uint32_t new_esp0);
int32_t load_executable_into_memory(executability_result_t exec_info, uint32_t nth_process);

void* translate_user_to_kernel(const void* user_addr, uint32_t nth_process);
void* translate_kernel_to_user(const void* kern_addr, uint32_t nth_process);

extern void process_init();
extern pcb_t* process_allocate(uint32_t parent);
extern int process_free(uint32_t pid);
void close_pid_fds(uint32_t pid);

extern uint32_t get_current_pid();
uint32_t get_canonical_pid(uint32_t pid);
extern pcb_t* get_current_pcb();
extern pcb_t* get_pcb(uint32_t pid);

int32_t save_context_in_pcb(
    pcb_t* this_pcb, 
    const from_kernel_context_t* this_kstack_context, 
    const hwcontext_t* optional_this_hw_context
);

int32_t initialize_kstack_context(
    from_kernel_context_t* kstack_context, 
    uint32_t init_user_eip, 
    eflags_register_fmt_t inherited_flags, 
    uint32_t init_user_esp
);

int is_kernel_pid(uint32_t pid);
int is_root_pid(uint32_t pid);

uint32_t derive_pid_from_esp();
int32_t derive_pid_from_tss();

#endif
