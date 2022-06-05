#ifndef SYSCALL_H
#define SYSCALL_H
#include "../x86_desc.h"

int32_t sys_halt(hwcontext_t* context);
int32_t sys_read(hwcontext_t* context);
int32_t sys_write(hwcontext_t* context);
int32_t sys_open(hwcontext_t* context);
int32_t sys_close(hwcontext_t* context);
int32_t sys_getargs(hwcontext_t* context);
int32_t sys_vidmap(hwcontext_t* context);
int32_t sys_set_handler(hwcontext_t* context);
int32_t sys_sigreturn(hwcontext_t* context);
int32_t syscall_prologue();
int32_t syscall_epilogue();

typedef struct manual_register_restore_t {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint16_t ds;
    uint16_t _pad_ds;
} __attribute__((packed)) manual_register_restore_t;

typedef struct pusha_context_t {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    // Do not change this! the POP_INTO_KERNEL_CONTEXT macro will
    // not work if you alter this!
    uint32_t esp_DO_NOT_CHANGE;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
} __attribute__((packed)) pusha_context_t;

typedef struct from_kernel_context_t {
    pusha_context_t pusha_context;
    uint16_t ds;
    uint16_t _pad_ds;
    iret_context_t iret_context;
} __attribute__((packed)) from_kernel_context_t;
#endif
