#ifndef SYS_HALT_H
#define SYS_HALT_H

#include "../x86_desc.h"
#include "syscall.h"

int32_t sys_halt_helper(
    hwcontext_t* caller_context,
    from_kernel_context_t* kstack_context
);

#endif
