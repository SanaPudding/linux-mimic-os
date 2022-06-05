#ifndef PAGING_H
#define PAGING_H

#include "types.h"
#include "common.h"

/* Number of entries in the PDE and the PTE */
#define NUM_PAGE_ENTRIES        1024

// Useful for loading an offset into a 4KB page table
#define GET_20_MSB(addr) (((addr) & 0xFFFFF000) >> 12)

// Useful for loading an offset into a 4MB page
#define GET_10_MSB(addr) (((addr) & 0xFFC00000) >> 22)

// Linear address: 
//      AAAAAAAAAA BBBBBBBBBB CCCCCCCCCCCC
#define GET_4KB_OFFSET_HIGH(addr)   (((addr) & 0xFFC00000) >> 22)
#define GET_4KB_OFFSET_MIDDLE(addr) (((addr) & 0x003FF000) >> 12)
#define GET_4KB_OFFSET_LOW(addr)    (((addr) & 0x00000FFF))

// Linear address:
//      AAAAAAAAAA BBBBBBBBBBBBBBBBBBBBBB
#define GET_4MB_OFFSET_HIGH(addr) (((addr) & 0xFFC00000) >> 22)
#define GET_ADDR_FROM_4MB_OFFSET_HIGH(offset) (((offset) & 0x3FF) << 22)
#define GET_4MB_OFFSET_LOW(addr)  ( (addr) & 0x003FFFFF )

#ifndef ASM

// Union to represent the CR0 register format without using masks.
// PLEASE use this for readability of code.
typedef union {
    uint32_t bits;
    struct {
        uint32_t pe : 1;
        uint32_t mp : 1;
        uint32_t em : 1;
        uint32_t ts : 1;
        uint32_t et : 1;
        uint32_t ne : 1;
        uint32_t reserved_6 : 10; 
        uint32_t wp : 1;
        uint32_t reserved_17 : 1;
        uint32_t am : 1;
        uint32_t reserved_19 : 10 ;
        uint32_t nw : 1;
        uint32_t cd : 1;
        uint32_t pg : 1;
    } __attribute__((packed));
} cr0_register_fmt;

// Union to represent the CR3 register format without using masks.
// PLEASE use this for readability of code.
typedef union {
    uint32_t bits;
    struct {
        uint32_t reserved_0 : 3;
        uint32_t pwt : 1;
        uint32_t pcd : 1;
        uint32_t reserved_5 : 7;
        uint32_t page_directory_base : 20;
    } __attribute__((packed));
} cr3_register_fmt;

// Union to represent the CR4 register format without using masks.
// PLEASE use this for readability of code.
typedef struct {
    uint32_t bits;
    struct {
        uint32_t vme : 1;
        uint32_t pvi : 1;
        uint32_t tsd : 1;
        uint32_t de : 1;
        uint32_t pse : 1;
        uint32_t pae : 1;
        uint32_t mce : 1; 
        uint32_t pge : 1;
        uint32_t pce : 1;
        uint32_t osfxsr : 1;
        uint32_t osxmmexcpt : 1;
        uint32_t reserved : 21;
    } __attribute__((packed));
} cr4_register_fmt;

// Explanation of variable names:
//      These variable names have instructions, which are pulled
//      directly from the intel manual. Semantics for how ECE 391
//      wants us to configure paging are not included here, because they
//      could potentially be variable.

// Struct representing a PDE pointing to a 4kb page table.
typedef struct to_4kb_page_table {
    uint32_t present_4kbtab : 1;
    uint32_t read_write_4kbtab : 1;
    uint32_t user_supervisor_4kbtab : 1;
    uint32_t writethrough_4kbtab : 1;
    uint32_t cache_disabled_4kbtab : 1;
    uint32_t accessed_4kbtab : 1;
    uint32_t reserved_set_to_zero_4kbtab : 1;
    uint32_t page_size_set_to_zero_4kbtab : 1;
    uint32_t global_page_ignore_4kbtab : 1;
    uint32_t custom_4kbtab : 3;
    uint32_t base_addr_4kbtab : 20;
} __attribute__ ((packed)) pde_4kb_pagetable_t;

// Struct representing a PDE pointing to a 4MB page.
typedef struct to_4mb_page {
    uint32_t present_4mb : 1;
    uint32_t read_write_4mb : 1;
    uint32_t user_supervisor_4mb : 1;
    uint32_t writethrough_4mb : 1;
    uint32_t cache_disabled_4mb : 1; 
    uint32_t accessed_4mb : 1;
    uint32_t dirty_4mb : 1;
    uint32_t page_size_set_to_one_4mb : 1;
    uint32_t global_4mb : 1;
    uint32_t custom_4mb : 3;
    uint32_t page_table_attr_4mb : 1;
    uint32_t reserved_set_to_zero_4mb : 9;
    uint32_t base_addr_4mb : 10;
} __attribute__ ((packed)) pde_4mb_page_t;

typedef struct to_unknown_page {
    uint32_t present : 1;
    uint32_t _unknown_1 : 6;
    uint32_t is_4mb : 1;
    uint32_t _unknown_2 : 24;
} __attribute__ ((packed)) pde_unknown_page_t;

typedef union page_directory_entry_t { 
        // Either points to a 4kb page table...
        pde_4kb_pagetable_t entry_to_4kb_table;
        // ..or to a 4mb page.
        pde_4mb_page_t entry_to_4mb_page;
        // ..or to a page we don't know
        pde_unknown_page_t entry_to_unknown_page;
} page_directory_entry_t;

typedef struct page_table_entry_t {
    uint32_t present : 1;
    uint32_t read_write : 1;
    uint32_t user_supervisor : 1;
    uint32_t writethrough : 1;
    uint32_t cache_disabled : 1;
    uint32_t accessed : 1;
    uint32_t dirty : 1;
    uint32_t page_table_attr : 1;
    uint32_t global : 1;
    uint32_t custom : 3;
    uint32_t base_addr : 20;
} __attribute__ ((packed)) page_table_entry_t;

#define TARGET_PROGRAM_LOCATION_VIRTUAL 0x08048000

// Where the userpage starts, from a physical (true) perspective
#define BEGINNING_USERPAGE_PHYSICAL_ADDR (8 * ONE_MB)
// Where the userpage starts, from a virtual (user's) perspective
#define BEGINNING_USERPAGE_VIRTUAL_ADDR (128 * ONE_MB)
#define BEGINNING_USERVID_VIRTUAL_ADDR 0xC0000

#define KERN_BEGIN_ADDR         0x400000
#define VIDMEM_KERN_BEGIN_ADDR  0xB8000

#define NUM_VMEM_PAGE 4

#define KERN_VMEM_PHYSICAL_BEGIN_ADDR           0xB8000
#define BACKGROUND_VMEM_PHYSICAL_BEGIN_ADDR_T1  0xB9000
#define BACKGROUND_VMEM_PHYSICAL_BEGIN_ADDR_T2  0xBA000
#define BACKGROUND_VMEM_PHYSICAL_BEGIN_ADDR_T3  0xBB000

#define SIZEOF_PROGRAMPAGE (4 * ONE_MB)
#define SIZEOF_4KBPAGE (4 * ONE_KB)

// Don't let our custom vidmap address map to the null page
STATIC_ASSERT(!(
    BEGINNING_USERVID_VIRTUAL_ADDR >= 0 && 
    BEGINNING_USERVID_VIRTUAL_ADDR < SIZEOF_4KBPAGE)
);

// Do not map to the kernel's vmem page
STATIC_ASSERT(!(
    (BEGINNING_USERVID_VIRTUAL_ADDR >= BEGINNING_USERPAGE_VIRTUAL_ADDR)
    && 
    (BEGINNING_USERVID_VIRTUAL_ADDR < BEGINNING_USERPAGE_VIRTUAL_ADDR + SIZEOF_4KBPAGE)
));

// Do not map to the program page
STATIC_ASSERT(!(
    (BEGINNING_USERVID_VIRTUAL_ADDR >= BEGINNING_USERPAGE_VIRTUAL_ADDR)
    &&
    (BEGINNING_USERVID_VIRTUAL_ADDR < BEGINNING_USERPAGE_VIRTUAL_ADDR + SIZEOF_PROGRAMPAGE)
));

// Do not map to the kernel program page
STATIC_ASSERT(!(
    (BEGINNING_USERVID_VIRTUAL_ADDR >= KERN_BEGIN_ADDR)
    &&
    (BEGINNING_USERVID_VIRTUAL_ADDR < KERN_BEGIN_ADDR + SIZEOF_PROGRAMPAGE)
));

extern page_directory_entry_t kernel_page_descriptor_table[NUM_PAGE_ENTRIES];   // 4kb
extern page_directory_entry_t user_page_descriptor_table[NUM_PAGE_ENTRIES];     // 4kb
extern page_table_entry_t kernel_vmem_page_table[NUM_PAGE_ENTRIES];             // 4kb
extern page_table_entry_t user_vmem_page_table[NUM_PAGE_ENTRIES];               // 4kb

#define PAGING_MAX_PID 7
typedef struct proc_paging_state_t {
    uint8_t user_vidmem_active;
    uint32_t current_mapped_pid;
    page_directory_entry_t* active_pde;
} proc_paging_state_t;

extern proc_paging_state_t active_paging_state;
void paging_init(void);

int32_t set_new_cr3(uint32_t new_pd_addr);
void flush_tlb();

int32_t destroy_user_programpage(int32_t nth_process);
int32_t create_new_user_programpage(int32_t nth_process);
int32_t activate_existing_user_programpage(int32_t pid);

int32_t is_unsafe_page_walk(void* addr);

int32_t activate_user_vidmem();
int32_t deactivate_user_vidmem();

int32_t set_user_vmem_base_addr(uint32_t addr);
uint32_t get_default_bgvmem_begin_addr(int32_t tid);

proc_paging_state_t init_root_proc_paging_state(uint32_t pid);
void load_paging_state_to_universe(proc_paging_state_t state);
proc_paging_state_t current_universe_paging_state();

#endif /* ASM */
#endif
