/* x86_desc.h - Defines for various x86 descriptors, descriptor tables,
 * and selectors
 * vim:ts=4 noexpandtab
 */

#ifndef _X86_DESC_H
#define _X86_DESC_H

#include "types.h"
#include "idt.h"
#include "common.h"

/* Segment selector values */
#define KERNEL_CS   0x0010
#define KERNEL_DS   0x0018
#define USER_CS     0x0023
#define USER_DS     0x002B
#define KERNEL_TSS  0x0030
#define KERNEL_LDT  0x0038

/* Size of the task state segment (TSS) */
#define TSS_SIZE    104

/* Number of vectors in the interrupt descriptor table (IDT) */
#define NUM_VEC     256

#ifndef ASM

/* This structure is used to load descriptor base registers
 * like the GDTR and IDTR */
typedef struct x86_desc {
    uint16_t padding;
    uint16_t size;
    uint32_t addr;
} x86_desc_t;

/* This is a segment descriptor.  It goes in the GDT. */
typedef struct seg_desc {
    union {
        uint32_t val[2];
        struct {
            uint16_t seg_lim_15_00;
            uint16_t base_15_00;
            uint8_t  base_23_16;
            uint32_t type          : 4;
            uint32_t sys           : 1;
            uint32_t dpl           : 2;
            uint32_t present       : 1;
            uint32_t seg_lim_19_16 : 4;
            uint32_t avail         : 1;
            uint32_t reserved      : 1;
            uint32_t opsize        : 1;
            uint32_t granularity   : 1;
            uint8_t  base_31_24;
        } __attribute__ ((packed));
    };
} seg_desc_t;

/* TSS structure */
typedef struct __attribute__((packed)) tss_t {
    uint16_t prev_task_link;
    uint16_t prev_task_link_pad;

    uint32_t esp0;
    uint16_t ss0;
    uint16_t ss0_pad;

    uint32_t esp1;
    uint16_t ss1;
    uint16_t ss1_pad;

    uint32_t esp2;
    uint16_t ss2;
    uint16_t ss2_pad;

    uint32_t cr3;

    uint32_t eip;
    uint32_t eflags;

    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;

    uint16_t es;
    uint16_t es_pad;

    uint16_t cs;
    uint16_t cs_pad;

    uint16_t ss;
    uint16_t ss_pad;

    uint16_t ds;
    uint16_t ds_pad;

    uint16_t fs;
    uint16_t fs_pad;

    uint16_t gs;
    uint16_t gs_pad;

    uint16_t ldt_segment_selector;
    uint16_t ldt_pad;

    uint16_t debug_trap : 1;
    uint16_t io_pad     : 15;
    uint16_t io_base_addr;
} tss_t;

/* Some external descriptors declared in .S files */
extern x86_desc_t gdt_desc;

extern uint16_t ldt_desc;
extern uint32_t ldt_size;
extern seg_desc_t ldt_desc_ptr;
extern seg_desc_t gdt_ptr;
extern uint32_t ldt;

extern uint32_t tss_size;
extern seg_desc_t tss_desc_ptr;
extern tss_t tss;

/* Sets runtime-settable parameters in the GDT entry for the LDT */
#define SET_LDT_PARAMS(str, addr, lim)                          \
do {                                                            \
    str.base_31_24 = ((uint32_t)(addr) & 0xFF000000) >> 24;     \
    str.base_23_16 = ((uint32_t)(addr) & 0x00FF0000) >> 16;     \
    str.base_15_00 = (uint32_t)(addr) & 0x0000FFFF;             \
    str.seg_lim_19_16 = ((lim) & 0x000F0000) >> 16;             \
    str.seg_lim_15_00 = (lim) & 0x0000FFFF;                     \
} while (0)

/* Sets runtime parameters for the TSS */
#define SET_TSS_PARAMS(str, addr, lim)                          \
do {                                                            \
    str.base_31_24 = ((uint32_t)(addr) & 0xFF000000) >> 24;     \
    str.base_23_16 = ((uint32_t)(addr) & 0x00FF0000) >> 16;     \
    str.base_15_00 = (uint32_t)(addr) & 0x0000FFFF;             \
    str.seg_lim_19_16 = ((lim) & 0x000F0000) >> 16;             \
    str.seg_lim_15_00 = (lim) & 0x0000FFFF;                     \
} while (0)

/* An interrupt descriptor entry (goes into the IDT) */
typedef union idt_desc_t {
    uint32_t val[2];
    struct {
        uint16_t offset_15_00;
        uint16_t seg_selector;
        uint8_t  reserved4;
        uint32_t reserved3 : 1;
        uint32_t reserved2 : 1;
        uint32_t reserved1 : 1;
        uint32_t size      : 1;
        uint32_t reserved0 : 1;
        uint32_t dpl       : 2;
        uint32_t present   : 1;
        uint16_t offset_31_16;
    } __attribute__ ((packed));
} idt_desc_t;

/* The IDT itself (declared in x86_desc.S */
extern idt_desc_t idt[NUM_VEC];
/* The descriptor used to load the IDTR */
extern x86_desc_t idt_desc_ptr;

/* Sets runtime parameters for an IDT entry */
#define SET_IDT_ENTRY(str, handler)                              \
do {                                                             \
    str.offset_31_16 = ((uint32_t)(handler) & 0xFFFF0000) >> 16; \
    str.offset_15_00 = ((uint32_t)(handler) & 0xFFFF);           \
} while (0)

/* Load task register.  This macro takes a 16-bit index into the GDT,
 * which points to the TSS entry.  x86 then reads the GDT's TSS
 * descriptor and loads the base address specified in that descriptor
 * into the task register */
#define ltr(desc)                       \
do {                                    \
    asm volatile ("ltr %w0"             \
            :                           \
            : "r" (desc)                \
            : "memory", "cc"            \
    );                                  \
} while (0)

/* Load the interrupt descriptor table (IDT).  This macro takes a 32-bit
 * address which points to a 6-byte structure.  The 6-byte structure
 * (defined as "struct x86_desc" above) contains a 2-byte size field
 * specifying the size of the IDT, and a 4-byte address field specifying
 * the base address of the IDT. */
#define lidt(desc)                      \
do {                                    \
    asm volatile ("lidt (%0)"           \
            :                           \
            : "g" (desc)                \
            : "memory"                  \
    );                                  \
} while (0)

/* Load the local descriptor table (LDT) register.  This macro takes a
 * 16-bit index into the GDT, which points to the LDT entry.  x86 then
 * reads the GDT's LDT descriptor and loads the base address specified
 * in that descriptor into the LDT register */
#define lldt(desc)                      \
do {                                    \
    asm volatile ("lldt %%ax"           \
            :                           \
            : "a" (desc)                \
            : "memory"                  \
    );                                  \
} while (0)

/* asm_wrappers for exception handlers */
extern void IDT_ASM_WRAPPER(IDT_DIVERR)(void);
extern void IDT_ASM_WRAPPER(IDT_INTEL_RESERVED)(void);
extern void IDT_ASM_WRAPPER(IDT_NMIINT)(void);
extern void IDT_ASM_WRAPPER(IDT_BREAK)(void);
extern void IDT_ASM_WRAPPER(IDT_OVERFLOW)(void);
extern void IDT_ASM_WRAPPER(IDT_BOUND)(void);
extern void IDT_ASM_WRAPPER(IDT_INVALOP)(void);
extern void IDT_ASM_WRAPPER(IDT_DEVICENA)(void);
extern void IDT_ASM_WRAPPER(IDT_DOUBLEFAULT)(uint32_t);
extern void IDT_ASM_WRAPPER(IDT_SEGMENT_OVERRUN_RESERVED)(void);
extern void IDT_ASM_WRAPPER(IDT_INVALTSS)(uint32_t);
extern void IDT_ASM_WRAPPER(IDT_SEGNOTPRESENT)(uint32_t);
extern void IDT_ASM_WRAPPER(IDT_STACKSEGFAULT)(uint32_t);
extern void IDT_ASM_WRAPPER(IDT_GENPROTECT)(uint32_t);
extern void IDT_ASM_WRAPPER(IDT_PAGEFAULT)(uint32_t);
extern void IDT_ASM_WRAPPER(IDT_INTEL_RESERVED_15)(uint32_t);
extern void IDT_ASM_WRAPPER(IDT_MATHFAULT)(void);
extern void IDT_ASM_WRAPPER(IDT_ALIGNCHK)(uint32_t);
extern void IDT_ASM_WRAPPER(IDT_MACHINECHK)(void);
extern void IDT_ASM_WRAPPER(IDT_SIMDFPE)(void);
extern void IDT_ASM_WRAPPER(IDT_SYSCALL)(void);

/* asm_wrappers for interrupts */
extern void keyboard_interrupt_wrapper();
extern void rtc_interrupt_wrapper();
extern void idt_asm_wrapper_pit();

/* asm_wrappers for system calls */
extern void idt_asm_wrapper_syscall();

// Struct for the eflags register - useful if you want to set the interrupt flag on a context switch
typedef union {
    uint32_t bits;
    struct {
        uint32_t cf : 1; // Carry flag
        uint32_t reserved_1_set_to_1 : 1;
        uint32_t pf : 1; // Parity flag
        uint32_t reserved_3_set_to_0 : 1;
        uint32_t af : 1; // Something?
        uint32_t reserved_5_set_to_0 : 1;
        uint32_t zf : 1; // Zero flag
        uint32_t sf : 1; // Sign flag
        uint32_t tf : 1; // Trap flag
        uint32_t int_f : 1; // Interrupt flag
        uint32_t df : 1; // Something?
        uint32_t of : 1; // Overflow flag?
        uint32_t iopl : 2; // IO privilege level
        uint32_t nt : 1; // Nested task flag
        uint32_t reserved_15_set_to_0 : 1;
        uint32_t rf : 1; // Resume flag
        uint32_t vm : 1; // Virtual 8086 mode
        uint32_t ac : 1; // Alignment check
        uint32_t vif : 1; // Virtual interrupt flag
        uint32_t vip : 1; // Virtual interrupt pending
        uint32_t id : 1; // Identification flag
        uint32_t reserved_22_set_to_zero : 10;
    } __attribute__((packed));
} eflags_register_fmt_t;

// Struct for the iret context - ESP and SS may not always represent valid values, 
// check against CS to see if we will return to kernel mode (in which case ESP and SS are invalid)
typedef struct iret_context_t {
    uint32_t ret_eip;
    uint16_t cs;
    uint16_t _pad_cs; // Little endian, bruh
    eflags_register_fmt_t eflags;
    uint32_t esp;
    uint16_t ss;
    uint16_t _pad_ss;
} __attribute__((packed)) iret_context_t;

// Struct for the hardware context as described in descriptors.pdf
typedef struct {
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
    uint32_t eax;
    uint16_t ds;
    uint16_t _pad_ds;
    uint16_t es;
    uint16_t _pad_es;
    uint32_t vecnum;
    uint32_t errcode;
    iret_context_t iret_context;
} __attribute__((packed)) hwcontext_t;

// Figure out next PID via TSS
typedef struct regs_hwcontext_t {
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
    uint32_t eax;
    uint16_t ds;
    uint16_t _pad_ds;
    uint16_t es;
    uint16_t _pad_es;
} __attribute__((packed)) regs_hwcontext_t;

typedef struct sched_hwcontext_t {
    regs_hwcontext_t regs_context;
    uint32_t* post_int_esp;
    iret_context_t iret_context;
} __attribute__((packed)) sched_hwcontext_t;

typedef struct exit_sched_to_k_context_t {
    regs_hwcontext_t regs_context;
    uint32_t* next_esp;
} __attribute__((packed)) exit_sched_to_k_context_t;

typedef struct exit_sched_to_u_context_t {
    regs_hwcontext_t regs_context;
    iret_context_t iret_context;
} __attribute__((packed)) exit_sched_to_u_context_t;

extern void common_interrupt_handler(hwcontext_t* context);
extern void common_exception_handler(hwcontext_t* context);

void dump_context(hwcontext_t context);
int32_t was_called_from_kernel(hwcontext_t* context);

// This is never used, don't use this garbage
static inline void set_ds_segment(uint16_t selector) {
    // Technically I'm not clobbering %esp because I'm immediately restoring it
    asm volatile (
        "pushw %[selector];"
        "popw %%ds"
        :
        : [selector] "m" (selector)
    );
}

static inline uint32_t get_cr3() {
    uint32_t cr3val;
    asm (
        "movl %%cr3, %%eax;"
        "movl %%eax, %[cr3val];"
        : [cr3val] "=m" (cr3val)
        :
        : "eax"
    );
    return cr3val;
}

static inline eflags_register_fmt_t get_eflags() {
    eflags_register_fmt_t flags;
    asm (
        "pushfl;"
        "popl %%eax;"
        "movl %%eax, %[flags]"
        : [flags] "=m" (flags.bits)
        :
        : "eax"
    );
    return flags;
}
/* Spin (nicely, so we don't chew up cycles) */
#define SPIN() while(1) {}

#endif /* ASM */

#endif /* _x86_DESC_H */
