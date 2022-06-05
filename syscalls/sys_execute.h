#ifndef SYS_EXECUTE_H
#define SYS_EXECUTE_H

#include "../x86_desc.h"
#include "syscall.h"

typedef struct execute_result_t {
        int32_t retval;
        uint8_t flag_allocated_proc;
        uint8_t flag_configured_paging;
        uint8_t flag_updated_esp0;
        uint32_t allocated_proc_id;
        uint32_t origin_proc_id;
} execute_result_t;

int32_t do_launch_from_kernel(const char* input, from_kernel_context_t* context);
int32_t entrypoint_launch_from_kernel(const char* input);

execute_result_t sys_execute_helper(
        hwcontext_t* caller_context,
        from_kernel_context_t* kstack_context
);
#endif
