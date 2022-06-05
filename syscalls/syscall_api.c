#include "syscall_api.h"

// NOTE: the user program won't be calling these functions, they're here for testing
// (and potentially for launching our first user program)

// Example of 1 arg
int32_t halt(uint8_t status) {
    int32_t retval;
    DO_SYSCALL_ZERO_ARGS(SYSCALL_NUM_HALT, retval);
    return retval;
}

int32_t execute(const uint8_t* command) {
    int32_t retval;
    DO_SYSCALL_ONE_ARG(SYSCALL_NUM_EXECUTE, retval, command);
    return retval;
}

// Example of 3 args
int32_t read(int32_t fd, void* buf, int32_t nbytes) {
    int32_t retval;
    DO_SYSCALL_THREE_ARGS(SYSCALL_NUM_READ, retval, fd, buf, nbytes);
    return retval;
}

int32_t write(int32_t fd, const void* buf, int32_t nbytes) {
    return -1;
}

int32_t open(const uint8_t* filename) {
    return -1;
}

int32_t close(int32_t fd) {
    return -1;
}

// Example of 2 args
int32_t getargs(uint8_t* buf, int32_t nbytes) {
    int32_t retval;
    DO_SYSCALL_TWO_ARGS(SYSCALL_NUM_GETARGS, retval, buf, nbytes);
    return retval;
}

int32_t vidmap(uint8_t** screen_start) {
    return -1;
}


int32_t set_handler(int32_t signum, void* handler_address) {
    return -1;
}

// Example of 0 arg
int32_t sigreturn(void) {
    int32_t retval;
    DO_SYSCALL_ZERO_ARGS(SYSCALL_NUM_SIGRETURN, retval);
    return retval;
}
