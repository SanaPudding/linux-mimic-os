/* kernel.c - the C part of the kernel
 * vim:ts=4 noexpandtab
 */

#include "multiboot.h"
#include "x86_desc.h"
#include "lib.h"
#include "debug.h"
#include "tests/tests.h"
#include "device-drivers/i8259.h"
#include "device-drivers/terminal.h"
#include "device-drivers/keyboard.h"
#include "device-drivers/rtc.h"
#include "device-drivers/pit.h"
#include "device-drivers/VGA.h"
#include "paging.h"
#include "idt.h"
#include "memfs/memfs.h"
#include "syscalls/syscall_api.h"
#include "syscalls/sys_execute.h"
#include "process/process.h"
#include "sched/sched.h"

/* Macros. */
/* Check if the bit BIT in FLAGS is set. */
#define CHECK_FLAG(flags, bit)   ((flags) & (1 << (bit)))

// Print all values of the passed in hwcontext_t, which comprises of processor-pushed args, vector number, pushed registers.
// Inputs: Object of type hwcontext_t
// Output: None
// Side effects: Dumps the value of the struct to the screen
void dump_context(hwcontext_t context) {
    printf("ebx      = %#x      \n", context.ebx);
    printf("ecx      = %#x      \n", context.ecx);
    printf("edx      = %#x      \n", context.edx);
    printf("esi      = %#x      \n", context.esi);
    printf("edi      = %#x      \n", context.edi);
    printf("ebp      = %#x      \n", context.ebp);
    printf("eax      = %#x      \n", context.eax);
    printf("xds      = %#x      \n", context.ds);
    printf("xes      = %#x      \n", context.es);
    printf("vecnum   = %#x      \n", context.vecnum);
    printf("errcode  = %#x      \n", context.errcode);
    printf("iret-eip = %#x      \n", context.iret_context.ret_eip);
    printf("iret-xcs = %#x      \n", context.iret_context.cs);
    printf("iret-flg = %#x      \n", context.iret_context.eflags.bits);
    printf("iret-esp = %#x      \n", context.iret_context.esp);
    printf("iret-xss = %#x      \n", context.iret_context.ss);
}

// Common syscall handler. 
// Inputs: Object of type hwcontext_t, the context of the handler.
// Output: None
// Side effect: Depends on the vector number of the context.

/* Check if MAGIC is valid and print the Multiboot information structure
   pointed by ADDR. */
void entry(unsigned long magic, unsigned long addr) {

    multiboot_info_t *mbi;
    module_t fs_mod; // mod struct to store fs information.
    
    /* Clear the screen. */
    clear();

    /* Am I booted by a Multiboot-compliant boot loader? */
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        printf("Invalid magic number: 0x%#x\n", (unsigned)magic);
        return;
    }

    /* Set MBI to the address of the Multiboot information structure. */
    mbi = (multiboot_info_t *) addr;

    /* Print out the flags. */
    printf("flags = 0x%#x\n", (unsigned)mbi->flags);

    /* Are mem_* valid? */
    if (CHECK_FLAG(mbi->flags, 0))
        printf("mem_lower = %uKB, mem_upper = %uKB\n", (unsigned)mbi->mem_lower, (unsigned)mbi->mem_upper);

    /* Is boot_device valid? */
    if (CHECK_FLAG(mbi->flags, 1))
        printf("boot_device = 0x%#x\n", (unsigned)mbi->boot_device);

    /* Is the command line passed? */
    if (CHECK_FLAG(mbi->flags, 2))
        printf("cmdline = %s\n", (char *)mbi->cmdline);

    if (CHECK_FLAG(mbi->flags, 3)) {
        int mod_count = 0;
        int i;
        module_t* mod = (module_t*)mbi->mods_addr;
        while (mod_count < mbi->mods_count) {
            printf("Module %d loaded at address: 0x%#x\n", mod_count, (unsigned int)mod->mod_start);
            printf("Module %d ends at address: 0x%#x\n", mod_count, (unsigned int)mod->mod_end);
            printf("First few bytes of module:\n");
            for (i = 0; i < 16; i++) {
                printf("0x%x ", *((char*)(mod->mod_start+i)));
                if (i == 0) fs_mod = *mod; // Assume the first module is the filesystem.
            }
            printf("\n");
            mod_count++; 
            mod++;
        }
    }
    /* Bits 4 and 5 are mutually exclusive! */
    if (CHECK_FLAG(mbi->flags, 4) && CHECK_FLAG(mbi->flags, 5)) {
        printf("Both bits 4 and 5 are set.\n");
        return;
    }

    /* Is the section header table of ELF valid? */
    if (CHECK_FLAG(mbi->flags, 5)) {
        elf_section_header_table_t *elf_sec = &(mbi->elf_sec);
        printf("elf_sec: num = %u, size = 0x%#x, addr = 0x%#x, shndx = 0x%#x\n",
                (unsigned)elf_sec->num, (unsigned)elf_sec->size,
                (unsigned)elf_sec->addr, (unsigned)elf_sec->shndx);
    }

    /* Are mmap_* valid? */
    if (CHECK_FLAG(mbi->flags, 6)) {
        memory_map_t *mmap;
        printf("mmap_addr = 0x%#x, mmap_length = 0x%x\n",
                (unsigned)mbi->mmap_addr, (unsigned)mbi->mmap_length);
        for (mmap = (memory_map_t *)mbi->mmap_addr;
                (unsigned long)mmap < mbi->mmap_addr + mbi->mmap_length;
                mmap = (memory_map_t *)((unsigned long)mmap + mmap->size + sizeof (mmap->size)))
            printf("    size = 0x%x, base_addr = 0x%#x%#x\n    type = 0x%x,  length    = 0x%#x%#x\n",
                    (unsigned)mmap->size,
                    (unsigned)mmap->base_addr_high,
                    (unsigned)mmap->base_addr_low,
                    (unsigned)mmap->type,
                    (unsigned)mmap->length_high,
                    (unsigned)mmap->length_low);
    }

    /* Construct an LDT entry in the GDT */
    {
        seg_desc_t the_ldt_desc;
        the_ldt_desc.granularity = 0x0;
        the_ldt_desc.opsize      = 0x1;
        the_ldt_desc.reserved    = 0x0;
        the_ldt_desc.avail       = 0x0;
        the_ldt_desc.present     = 0x1;
        the_ldt_desc.dpl         = 0x0;
        the_ldt_desc.sys         = 0x0;
        the_ldt_desc.type        = 0x2;

        SET_LDT_PARAMS(the_ldt_desc, &ldt, ldt_size);
        ldt_desc_ptr = the_ldt_desc;
        lldt(KERNEL_LDT);
    }

    /* Construct a TSS entry in the GDT */
    {
        seg_desc_t the_tss_desc;
        the_tss_desc.granularity   = 0x0;
        the_tss_desc.opsize        = 0x0;
        the_tss_desc.reserved      = 0x0;
        the_tss_desc.avail         = 0x0;
        the_tss_desc.seg_lim_19_16 = TSS_SIZE & 0x000F0000;
        the_tss_desc.present       = 0x1;
        the_tss_desc.dpl           = 0x0;
        the_tss_desc.sys           = 0x0;
        the_tss_desc.type          = 0x9;
        the_tss_desc.seg_lim_15_00 = TSS_SIZE & 0x0000FFFF;

        SET_TSS_PARAMS(the_tss_desc, &tss, tss_size);

        tss_desc_ptr = the_tss_desc;

        tss.ldt_segment_selector = KERNEL_LDT;
        tss.ss0 = KERNEL_DS;
        tss.esp0 = 0x800000; // I think this was an arbitrary value
        ltr(KERNEL_TSS);
    }

    /* Initialize devices, memory, filesystem, enable device interrupts on the
     * PIC, any other initialization stuff... */

    /* Init IDT */
    idt_init();
    /* Init paging */
    paging_init();
    /* Init the PIC */
    i8259_init();
    /* Init the PIT */
    pit_init();
    /* Init the keyboard */
    keyboard_init();
    /* Init the terminals */
    terminal_init();
    /* Init the RTC */
    rtc_init();
    /* Init the PCBs */
    process_init();

    // Assuming the first module is the filesystem.
    fs_init(fs_mod);
    sched_init();
    printf("devices initialized\n");
    
    /* Enable interrupts */
    /* Do not enable the following until after you have set up your
     * IDT correctly otherwise QEMU will triple fault and simple close
     * without showing you any output */
    printf("Beginning OS...\n");
    sti();

    /* Run tests */
    // launch_tests();
    /* Execute the first program ("shell") ... */

    // printf("Welcome!\n");
    // int32_t val = entrypoint_launch_from_kernel("shell");
    
    asm volatile (".1: hlt; jmp .1;");
}
