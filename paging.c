#include "paging.h"
#include "process/process.h"
#include "lib.h"
#include "device-drivers/terminal.h"
#include "common.h"

proc_paging_state_t curr_proc_paging_state;

void enable_paging_c(uint32_t addr);
pde_4mb_page_t get_configured_pde4mb_for_kernel_code();
pde_4kb_pagetable_t get_configured_pde4kb_for_vmem(uint8_t supervisor_value, page_table_entry_t* vmem_page_table_addr);
int32_t initialize_user_page_directory();
int32_t initialize_kern_vidmem();

static int32_t is_valid_vmem_physical_begin_addr(uint32_t addr);

const uint32_t vmem_begin_addrs[NUM_VMEM_PAGE] = {(const uint32_t) KERN_VMEM_PHYSICAL_BEGIN_ADDR,
                                                  (const uint32_t) BACKGROUND_VMEM_PHYSICAL_BEGIN_ADDR_T1,
                                                  (const uint32_t) BACKGROUND_VMEM_PHYSICAL_BEGIN_ADDR_T2,
                                                  (const uint32_t) BACKGROUND_VMEM_PHYSICAL_BEGIN_ADDR_T3};

// Set up the paging table for kernel memory and video memory.
// Inputs: none
// Outputs: none
// Side effects: 
//      After running this function, the paging table is configured.
//      Also all side effects of enable_paging_c apply.    
void paging_init() {
    uint32_t i;
    for (i = 0; i < NUM_PAGE_ENTRIES; i++) {
        // By default, no page/entry should be present.
        kernel_vmem_page_table[i].present = 0;
        kernel_page_descriptor_table[i].entry_to_4kb_table.present_4kbtab = 0;
        // All other bits are don't cares, so we won't initialize them.
    }

    // PDE for the 0-4MB containing video memory
    kernel_page_descriptor_table[0].entry_to_4kb_table = get_configured_pde4kb_for_vmem(0, kernel_vmem_page_table);
    // PDE for the 4-8MB containing kernel code
    kernel_page_descriptor_table[1].entry_to_4mb_page = get_configured_pde4mb_for_kernel_code();
    
    initialize_kern_vidmem();
    initialize_user_page_directory(user_page_descriptor_table);
    enable_paging_c((uint32_t)(kernel_page_descriptor_table));

    curr_proc_paging_state.user_vidmem_active = 0;
    curr_proc_paging_state.current_mapped_pid = 0;
    curr_proc_paging_state.active_pde = (page_directory_entry_t*)kernel_page_descriptor_table;
}

// Helper function which sets control register flags in order to configure paging.
// Called by paging_init.
// Inputs: Address of the paging directory, casted as an integer.
// Outputs: none.
// Side effects: Control registers are set and paging is officially enabled.
void enable_paging_c(uint32_t addr) {
    set_new_cr3(addr);
    cr0_register_fmt introduce_cr0;
    cr4_register_fmt introduce_cr4;

    introduce_cr0.bits = 0;
    introduce_cr0.pe = 1;
    introduce_cr0.pg = 1;

    introduce_cr4.bits = 0;
    introduce_cr4.pse = 1;

    asm (
        // Set CR4
        "movl %%cr4, %%eax;"
        "orl $0x10, %%eax;"
        "movl %%eax, %%cr4;"

        // Set paging bit (and protection bit) to enable paging
        "movl %%cr0, %%eax;" 
        "orl %[cr0setflgs], %%eax;" 
        "movl %%eax, %%cr0;" 
        
        :
        :   [cr4setflgs] "m" (introduce_cr4.bits),
            [cr0setflgs] "m" (introduce_cr0.bits)
        : "eax"
    );
}

// Flushes the TLB
// Inputs/Outputs: None
// Side effects: Flushes the TLB
void flush_tlb() {
    asm (
        "movl %%cr3, %%eax;"
        "movl %%eax, %%cr3"
        :
        :
        : "eax"
    );
}

// Gets a configured page directory entry pointing to a 4MB page for kernel code
// Inputs: None
// Outputs: A page directory entry representing the kernel code page
// Side effects: None
pde_4mb_page_t get_configured_pde4mb_for_kernel_code() {
    pde_4mb_page_t the_page;
    the_page.present_4mb = 1; // Is present
    the_page.read_write_4mb = 1; // Want to read/write
    the_page.user_supervisor_4mb = 0; // Kernels's page, so supervisor mode
    the_page.writethrough_4mb = 0; // Always want writeback
    the_page.cache_disabled_4mb = 1; // Cache because this is actual memory
    the_page.dirty_4mb = 0; // Not dirty (yet)
    the_page.page_size_set_to_one_4mb = 1;
    the_page.global_4mb = 1; // Many users one kernel
    the_page.page_table_attr_4mb = 0; // We're told to set this to zero.
    the_page.reserved_set_to_zero_4mb = 0;
    the_page.base_addr_4mb = GET_10_MSB(KERN_BEGIN_ADDR);
    return the_page;
}

// Gets a configured page directory entry pointing to a 4MB page for kernel code
// Inputs:
//      supervisor_value: 0 if this should point to kernel, 1 otherwise
//      vmem_page_table_addr: address to a page table pointed at by a page directory entry (physical address)
// Outputs: A page table entry which maps to vmem_page_table_addr
// Side effects: None
pde_4kb_pagetable_t get_configured_pde4kb_for_vmem(uint8_t supervisor_value, page_table_entry_t* vmem_page_table_addr) {
    pde_4kb_pagetable_t the_page;
    the_page.present_4kbtab = 1; // Is present
    the_page.read_write_4kbtab = 1; // Want to read/write.
    the_page.user_supervisor_4kbtab = supervisor_value ? 1 : 0;
    the_page.writethrough_4kbtab = 0; // Always want writeback
    the_page.cache_disabled_4kbtab = 0; // Nocache because memory/mapped IO isn't real memory
    the_page.reserved_set_to_zero_4kbtab = 0;
    the_page.page_size_set_to_zero_4kbtab = 0;
    the_page.base_addr_4kbtab = GET_20_MSB((uint32_t)vmem_page_table_addr);
    return the_page;
}

// Initializes the page directory entry and page table for accessing user memory
// Inputs: None
// Outputs: 0 success, -1 failure
// Side effects: updates user_page_descriptor_table, user_vmem_page_table
int32_t initialize_user_vidmem() {
    uint32_t pd_entry_idx = GET_4KB_OFFSET_HIGH(BEGINNING_USERVID_VIRTUAL_ADDR);
    uint32_t pt_entry_idx = GET_4KB_OFFSET_MIDDLE(BEGINNING_USERVID_VIRTUAL_ADDR);
    pde_4kb_pagetable_t entry_to_vmem_table = get_configured_pde4kb_for_vmem(1, user_vmem_page_table);
    user_page_descriptor_table[pd_entry_idx].entry_to_4kb_table = entry_to_vmem_table;
    user_vmem_page_table[pt_entry_idx].present = 0;
    user_vmem_page_table[pt_entry_idx].read_write = 1;
    user_vmem_page_table[pt_entry_idx].user_supervisor = 1; // User may access this page.
    user_vmem_page_table[pt_entry_idx].writethrough = 0;
    user_vmem_page_table[pt_entry_idx].cache_disabled = 0;
    user_vmem_page_table[pt_entry_idx].dirty = 0;
    user_vmem_page_table[pt_entry_idx].page_table_attr = 0;
    user_vmem_page_table[pt_entry_idx].global = 0; 
    user_vmem_page_table[pt_entry_idx].base_addr = GET_20_MSB(VIDMEM_KERN_BEGIN_ADDR); // Mapping to the kernel's video memory
    return 0;
}

// Function that initializes the video memory for the kernel, including the backing pages for video memory
// Inputs: None
// Outputs: 0
// Side effects: Sets up the kernel_vmem_page_table page table
int32_t initialize_kern_vidmem() {
    int32_t i;
    uint32_t pte_idx;
    for (i = 0; i < NUM_VMEM_PAGE; i++) {
        pte_idx = GET_4KB_OFFSET_MIDDLE(vmem_begin_addrs[i]);
        kernel_vmem_page_table[pte_idx].present = 1;
        kernel_vmem_page_table[pte_idx].read_write = 1;
        kernel_vmem_page_table[pte_idx].user_supervisor = 0; // Kernel's video memory only
        kernel_vmem_page_table[pte_idx].writethrough = 0;
        kernel_vmem_page_table[pte_idx].cache_disabled = 0;
        kernel_vmem_page_table[pte_idx].dirty = 0;
        kernel_vmem_page_table[pte_idx].page_table_attr = 0;
        kernel_vmem_page_table[pte_idx].global = 0; 
        kernel_vmem_page_table[pte_idx].base_addr = GET_20_MSB(vmem_begin_addrs[i]);
    }
    return 0;
}

// Activates the user video memory page table
// Inputs: none
// Outputs: 0 success, -1 failure
// Side effects: Marks the page for user video memory as present
int32_t activate_user_vidmem() {
    uint32_t flags, garbage;
    uint32_t pt_entry_idx = GET_4KB_OFFSET_MIDDLE(BEGINNING_USERVID_VIRTUAL_ADDR);
    CRITICAL_SECTION_FLAGSAVE(flags, garbage) {
        user_vmem_page_table[pt_entry_idx].present = 1;
        flush_tlb();
        curr_proc_paging_state.user_vidmem_active = 1;
    }
    return 0;
}

// Deactivates the user video memory page table
// Inputs: none
// Outputs: 0 success, -1 failure
// Side effects: Marks the page for user video memory as not present
int32_t deactivate_user_vidmem() {
    uint32_t flags, garbage;
    int32_t pt_entry_idx = GET_4KB_OFFSET_MIDDLE(BEGINNING_USERVID_VIRTUAL_ADDR);
    CRITICAL_SECTION_FLAGSAVE(flags, garbage) {
        user_vmem_page_table[pt_entry_idx].present = 0;
        flush_tlb();
        curr_proc_paging_state.user_vidmem_active = 0;
    }
    return 0;
}

// Creates all necessary pages for process memory for a new process
// Inputs: The PID of the new process
// Outputs: 0 success, -1 failure
// Side effects: Updates kernel_page_descriptor_table and user_page_descriptor_table and marks the appropriate pages as present
int32_t create_new_user_programpage(int32_t pid) {
    // Introduce two new pages:
    //      Page for user directory for user program page
    //      Page for kernel to access that user program page
    pde_4mb_page_t u_user_page;
    pde_4mb_page_t k_user_page;

    // Boring configuration according to descriptors.pdf, see paging_init for some explanation on these flags
    u_user_page.present_4mb = 1;
    u_user_page.read_write_4mb = 1;
    u_user_page.user_supervisor_4mb = 1; // For user and for kernel
    u_user_page.writethrough_4mb = 0;
    u_user_page.cache_disabled_4mb = 1;
    u_user_page.accessed_4mb = 0;
    u_user_page.dirty_4mb = 0;
    u_user_page.page_size_set_to_one_4mb = 1;
    u_user_page.global_4mb = 0;
    u_user_page.custom_4mb = 0;
    u_user_page.page_table_attr_4mb = 0;
    u_user_page.reserved_set_to_zero_4mb = 0;
    
    k_user_page.present_4mb = 1;
    k_user_page.read_write_4mb = 1;
    k_user_page.user_supervisor_4mb = 0; // For kernel only
    k_user_page.writethrough_4mb = 0;
    k_user_page.cache_disabled_4mb = 1;
    k_user_page.accessed_4mb = 0;
    k_user_page.dirty_4mb = 0;
    k_user_page.page_size_set_to_one_4mb = 1;
    k_user_page.global_4mb = 0;
    k_user_page.custom_4mb = 0;
    k_user_page.page_table_attr_4mb = 0;
    k_user_page.reserved_set_to_zero_4mb = 0;

    const uint32_t VIRTUAL_OFFSET_TO_MEM = GET_10_MSB(BEGINNING_USERPAGE_VIRTUAL_ADDR);
    const uint32_t PHYSICAL_OFFSET_TO_MEM = 
        GET_10_MSB(BEGINNING_USERPAGE_PHYSICAL_ADDR + (pid - 1) * SIZEOF_PROGRAMPAGE);

    uint32_t flags, garbage;
    CRITICAL_SECTION_FLAGSAVE(flags, garbage) {
        // If the kernel page was already present, our bookkeeping was bad - need to fail fast
        // Better to fail fast than to brownout and not figure out what the heck was going on
        // TODO: Set up a new entry in the kernel for a kernel panic
        PRINT_ASSERT(
            kernel_page_descriptor_table[PHYSICAL_OFFSET_TO_MEM].entry_to_4mb_page.present_4mb == 0
            , "Creating already present page for PID=%d!\n", pid
        );

        // Mapping virtual to physical
        u_user_page.base_addr_4mb = PHYSICAL_OFFSET_TO_MEM;

        // Map the virtual offset to the 
        // WARNING: If there are page fault bugs, check this
        user_page_descriptor_table[VIRTUAL_OFFSET_TO_MEM].entry_to_4mb_page = u_user_page;
        
        // One-to-one mapping
        k_user_page.base_addr_4mb = PHYSICAL_OFFSET_TO_MEM;
        kernel_page_descriptor_table[PHYSICAL_OFFSET_TO_MEM].entry_to_4mb_page = k_user_page;
    }
    return 0;
}

// Function to activate an existiung user program page
// Sets up the user page descriptor table to point to the existing physical memory
// Inputs: The PID to activate paging for
// Outputs: 0 success, -1 failure
// Side effects: Modifies user_page_descriptor_table, flushes TLB
int32_t activate_existing_user_programpage(int32_t pid) {
    if (is_kernel_pid(pid)) return 0; // PID 0 means we don't have to configure any program page -- just ignore.
    const uint32_t VIRTUAL_OFFSET_TO_MEM = GET_10_MSB(BEGINNING_USERPAGE_VIRTUAL_ADDR);
    const uint32_t PHYSICAL_OFFSET_TO_MEM = 
        GET_10_MSB(BEGINNING_USERPAGE_PHYSICAL_ADDR + (pid - 1) * SIZEOF_PROGRAMPAGE);
    
    // If it doesn't exist for the kernel, it doesn't exist for us
    if (!kernel_page_descriptor_table[PHYSICAL_OFFSET_TO_MEM].entry_to_4mb_page.present_4mb) {
            return -1;
    }

    uint32_t flags, garbage;
    CRITICAL_SECTION_FLAGSAVE(flags, garbage) {
        user_page_descriptor_table[VIRTUAL_OFFSET_TO_MEM].entry_to_4mb_page.base_addr_4mb = PHYSICAL_OFFSET_TO_MEM;
        user_page_descriptor_table[VIRTUAL_OFFSET_TO_MEM].entry_to_4mb_page.present_4mb = 1;
        curr_proc_paging_state.current_mapped_pid = pid;
        flush_tlb();
    }
    return 0;
}

// Function to destroy an existing user program page
// Marks the process page for the kernel as not present
// Inputs: The PID to delete paging for
// Outputs: 0 success, -1 failure, busy loop on inconsistency
// Side effects: Modifies kernel_page_descriptor_table, flushes TLB
int32_t destroy_user_programpage(int32_t pid) {
    const uint32_t PHYSICAL_OFFSET_TO_MEM = 
        GET_10_MSB(BEGINNING_USERPAGE_PHYSICAL_ADDR + (pid - 1) * SIZEOF_PROGRAMPAGE);
    
    uint32_t flags, garbage;
    CRITICAL_SECTION_FLAGSAVE(flags, garbage) {
        if (!kernel_page_descriptor_table[PHYSICAL_OFFSET_TO_MEM].entry_to_4mb_page.present_4mb) {
            printf("Deconfigure inconsistency!\n");
            while(1) { int y = 0; (void)y; }
        }
        kernel_page_descriptor_table[PHYSICAL_OFFSET_TO_MEM].entry_to_4mb_page.present_4mb = 0;
    }
    
    return 0;
}

// Function to initialize the user_page_descriptor_table to proper values
// Inputs: None
// Outputs: 0 success, -1 failure
// Side effects: modifies user_page_descriptor_table, all side effects of initialize_user_vidmem
int32_t initialize_user_page_directory() {
    uint32_t i;
    for (i = 0; i < NUM_PAGE_ENTRIES; i++) {
        user_page_descriptor_table[i].entry_to_4mb_page.present_4mb = 0;
    }

    user_page_descriptor_table[GET_10_MSB(KERN_BEGIN_ADDR)].entry_to_4mb_page 
        = get_configured_pde4mb_for_kernel_code();

    initialize_user_vidmem();

    return 0;
}

// Function to set the new value of cr3
// Inputs: Address of the new page directory to use
// Outputs: 0 success, -1 failure
// Side effects: Modifies the active CR3 and flushes TLB
int32_t set_new_cr3(uint32_t new_pd_addr) {
    cr3_register_fmt reserved_cr3_mask;
    cr3_register_fmt new_cr3;

    // The intel manual says that:
    //          When loading a control register, reserved bits 
    //          should always be set to the values previously read. (Below Fig 2-5)
    // This is therefore absolutely necessary.
    reserved_cr3_mask.bits = 0;
    reserved_cr3_mask.reserved_0 = ~0;
    reserved_cr3_mask.reserved_5 = ~0;

    // prep the page directory base value to load into CR3
    new_cr3.bits = 0;
    new_cr3.page_directory_base = GET_20_MSB(new_pd_addr);

    uint32_t flags, garbage;
    CRITICAL_SECTION_FLAGSAVE(flags, garbage) {
        asm (
            // Set CR3, preserving reserved bits.
            "movl %[new_cr3], %%eax;"
            "movl %%cr3, %%ecx;"
            "andl %[cr3_preserve], %%ecx;"
            "orl %%ecx, %%eax;" 
            "movl %%eax, %%cr3;"
            :
            :   [new_cr3] "m" (new_cr3.bits), 
                [cr3_preserve] "m" (reserved_cr3_mask)
            : "eax", "ecx"
        );
        flush_tlb();
        curr_proc_paging_state.active_pde = (page_directory_entry_t*)new_pd_addr;
    }
    return 0;
}

// Function to emulate pagewalking
// Inputs: a generic pointer representing an address
// Outputs: 0 if safe, -1 if unsafe
// Side effects: None
int32_t is_unsafe_page_walk(void* addr) {
    cr3_register_fmt cr3;
    asm (
        "movl %%cr3, %%eax;"
        "movl %%eax, %[cr3]"
        : [cr3] "=g" (cr3.bits)
        :
        : "eax"
    );

    uint32_t pde_offset = GET_10_MSB((uint32_t)addr);
    page_directory_entry_t* the_directory = (page_directory_entry_t*)(cr3.page_directory_base << 12);
    if (the_directory[pde_offset].entry_to_unknown_page.present) {
        if (the_directory[pde_offset].entry_to_unknown_page.is_4mb) {
            return 0;
        } else {
            uint32_t pt_offset = GET_4KB_OFFSET_MIDDLE((uint32_t)(addr));
            page_table_entry_t* the_table 
                = (page_table_entry_t*)(the_directory[pde_offset].entry_to_4kb_table.base_addr_4kbtab << 12);
            if (the_table[pt_offset].present) {
                return 0;
            } else {
                return -1;
            }
        }
    } else {
        return -1;
    }
    return -1;
}

// Function that returns the default begin address of backing video memory
// corresponds to a specific terminal.
// Inputs: tid -- a termianl id.
// Outputs: begin address of a backing vmem upon success, 0 upon failure.
uint32_t get_default_bgvmem_begin_addr(int32_t tid) {
    if (tid < 0 || tid > MAX_NUM_TERMINAL) {return 0;}
    return vmem_begin_addrs[tid + 1];
}

// Function that sets user virtual video memory mapping.
// Inputs: addr -- physical begin address of a video memory page.
// Outputs: 0 upon success, -1 upon failure.
int32_t set_user_vmem_base_addr(uint32_t addr) {
    if (!is_valid_vmem_physical_begin_addr(addr)) {return -1;}

    uint32_t pt_idx = GET_4KB_OFFSET_MIDDLE(BEGINNING_USERVID_VIRTUAL_ADDR);
    user_vmem_page_table[pt_idx].base_addr = GET_20_MSB(addr);
    flush_tlb();
    return 0;
}

// Function that checks if a given address is a valid vmem begin address.
// Inputs: addr -- a input vmem begin address
// Outputs: 1 if valid, 0 if not.
static int32_t is_valid_vmem_physical_begin_addr(uint32_t addr) {
    switch (addr)
    {
    case KERN_VMEM_PHYSICAL_BEGIN_ADDR:
        return 1;
    case BACKGROUND_VMEM_PHYSICAL_BEGIN_ADDR_T1:
        return 1;
    case BACKGROUND_VMEM_PHYSICAL_BEGIN_ADDR_T2:
        return 1;
    case BACKGROUND_VMEM_PHYSICAL_BEGIN_ADDR_T3:
        return 1;
    default:
        return 0;
    }
}

// Function that loads a given process paging state to the CPU and page tables; necessary for the scheduler
// Inputs: 
//      state: state of the paging state to restore
// Outputs: None
// Side effects: Activates or deactivates the user video memory, modifies the necessary CR3, and maps the program page if necessary
void load_paging_state_to_universe(proc_paging_state_t state) {
    if (state.user_vidmem_active) {
        activate_user_vidmem();
    } else {
        deactivate_user_vidmem();
    }
    
    if (state.current_mapped_pid) {
        activate_existing_user_programpage(state.current_mapped_pid);
    }

    set_new_cr3((uint32_t)state.active_pde);
    curr_proc_paging_state = state;
}

// Function that returns the current paging state that the computer is taking upon
// Inputs: None
// Outputs: The paginig state as a struct
proc_paging_state_t current_universe_paging_state() {
    return curr_proc_paging_state;
}

// Gets an initial paging state of a universe for a fresh new PID
// Inputs: PID of the initial process
// Outputs: PID
// Side effects: None
proc_paging_state_t init_root_proc_paging_state(uint32_t pid) {
    proc_paging_state_t new_state;
    new_state.user_vidmem_active = 0;
    new_state.current_mapped_pid = pid;
    new_state.active_pde = (page_directory_entry_t*)user_page_descriptor_table;
    return new_state;
}
