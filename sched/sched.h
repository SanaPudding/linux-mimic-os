#ifndef SCHED_H
#define SCHED_H

#include "../x86_desc.h"

#define NUM_SIMULTANEOUS_PROCS 3

#ifndef ASM

void exit_sched_to_k();
void exit_sched_to_u();

// C helpers called by the ASM functions
void exit_sched_to_k_helper(exit_sched_to_k_context_t* fill_context);
void exit_sched_to_u_helper(exit_sched_to_u_context_t* fill_context);

void schedule_failed(int retval);
int sched_init();

#endif
#endif
