#ifndef SYSCALL_API_H
#define SYSCALL_API_H
#include "../types.h"

int32_t halt(uint8_t status);
int32_t execute(const uint8_t* command);
int32_t read(int32_t fd, void* buf, int32_t nbytes);
int32_t write(int32_t fd, const void* buf, int32_t nbytes);
int32_t open(const uint8_t* filename);
int32_t close(int32_t fd);
int32_t getargs(uint8_t* buf, int32_t nbytes);
int32_t vidmap(uint8_t** screen_start);
int32_t set_handler(int32_t signum, void* handler_address);
int32_t sigreturn(void);

#define SYSCALL_NUM_HALT 1
#define SYSCALL_NUM_EXECUTE 2
#define SYSCALL_NUM_READ 3
#define SYSCALL_NUM_WRITE 4
#define SYSCALL_NUM_OPEN 5
#define SYSCALL_NUM_CLOSE 6
#define SYSCALL_NUM_GETARGS 7
#define SYSCALL_NUM_VIDMAP 8
#define SYSCALL_NUM_SET_HANDLER 9
#define SYSCALL_NUM_SIGRETURN 10

// Comments on macros:
// Mark all ASM as volatile, because there's no knowing what memory a syscall might change
// Saving EBX is necessary because of the C calling convention - but GCC will do that for us 
// because we tell it that we will clobber EBX.

#define DO_SYSCALL_ZERO_ARGS(syscall_number, output_variable) \
    asm volatile ( \
        "movl %[sysnum], %%eax;" \
        "int $0x80;" \
        "movl %%eax, %[retval]" \
        : [retval] "=m" ( output_variable ) \
        : [sysnum] "g" ( syscall_number ) \
        : "eax" \
    )

#define DO_SYSCALL_ONE_ARG(syscall_number, output_variable, c_arg1) \
    asm volatile ( \
        "movl %[sysnum], %%eax;" \
        "movl %[arg1], %%ebx;" \
        "int $0x80;" \
        "movl %%eax, %[retval]" \
        :   [retval] "=m" ( output_variable ) \
        :   [sysnum] "g" ( syscall_number ), \
            [arg1] "g" ( c_arg1 ) \
        : "eax", "ebx" \
    )

#define DO_SYSCALL_TWO_ARGS(syscall_number, output_variable, c_arg1, c_arg2) \
    asm volatile ( \
        "movl %[sysnum], %%eax;" \
        "movl %[arg1], %%ebx;" \
        "movl %[arg2], %%ecx;" \
        "int $0x80;" \
        "movl %%eax, %[retval]" \
        :   [retval] "=m" ( output_variable ) \
        :   [sysnum] "g" ( syscall_number ), \
            [arg1] "g" ( c_arg1 ), \
            [arg2] "g" ( c_arg2 ) \
        : "eax", "ebx", "ecx" \
    )

#define DO_SYSCALL_THREE_ARGS(syscall_number, output_variable, c_arg1, c_arg2, c_arg3) \
    asm volatile ( \
        "movl %[sysnum], %%eax;" \
        "movl %[arg1], %%ebx;" \
        "movl %[arg2], %%ecx;" \
        "movl %[arg3], %%edx;" \
        "int $0x80;" \
        "movl %%eax, %[retval]" \
        :   [retval] "=m" ( output_variable ) \
        :   [sysnum] "g" ( syscall_number ), \
            [arg1] "g" (c_arg1), \
            [arg2] "g" (c_arg2), \
            [arg3] "g" (c_arg3) \
        : "eax", "ebx", "ecx", "edx" \
    )

#define SYSCALL_0_ARGS
#define SYSCALL_1_ARGS
#define SYSCALL_2_ARGS
#define SYSCALL_3_ARGS
#endif
