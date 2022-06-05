#include "tests.h"
#include "../common.h"
#include "../x86_desc.h"
#include "../idt.h"
#include "../paging.h"
#include "../device-drivers/keyboard.h"
#include "../device-drivers/terminal.h"
#include "../device-drivers/rtc.h"
#include "../memfs/memfs.h"

static inline void assertion_failure(){
	/* Use exception #15 for assertions, otherwise
	   reserved by Intel */
	// asm volatile("int $15");
}

/* Checkpoint 1 tests */

STATIC_ASSERT(sizeof(page_directory_entry_t) == sizeof(uint32_t));
STATIC_ASSERT(sizeof(kernel_page_descriptor_table) == 4 * NUM_PAGE_ENTRIES);
STATIC_ASSERT(sizeof(page_table_entry_t) == sizeof(uint32_t));
STATIC_ASSERT(sizeof(kernel_vmem_page_table) == 4 * NUM_PAGE_ENTRIES);


/* IDT Test - Example
 * 
 * Asserts that first 10 IDT entries are not NULL
 * Inputs: None
 * Outputs: PASS/FAIL
 * Side Effects: None
 * Coverage: Load IDT, IDT definition
 * Files: x86_desc.h/S
 */
int test_populated_idt(){
	TEST_HEADER;

	int i;
	int result = PASS;
	for (i = 0; i < 20; ++i){
		if ((idt[i].offset_15_00 == NULL) && 
			(idt[i].offset_31_16 == NULL)){
			assertion_failure();
			result = FAIL;
		}
	}

	if ((idt[IDT_KEYBOARD].offset_15_00 == NULL) && 
		(idt[IDT_KEYBOARD].offset_31_16 == NULL)) {
			assertion_failure();
			result = FAIL;
	}

	return result;
}

// Native exception test
// Tests a hardware-invoked division zero error
// Inputs: none
// Outputs: FAIL on exit
// Side FX: Call IDT callback for division zero exception.
int test_exception_divzero() {
	TEST_HEADER;
	// Magic trick to divide by zero, the compiler will never know!
	uint32_t a_number = ((uint32_t)idt) >> 31;
	a_number = a_number & 0x0;
	a_number = 1 / a_number;
	return FAIL;
}

// Tests a software-invoked exception of type TEST_EXCEPTION_VECTOR
// Inputs: none
// Outputs: FAIL on exit
// Side FX: Call IDT callback for TEST_EXCEPTION_VECTOR exception.
#define TEST_EXCEPTION_VECTOR IDT_PAGEFAULT
int idt_exception_arbitrary() {
	TEST_HEADER;
	EMULATE_RET_EXCEPTION(IDT_PAGEFAULT, 0);
	return FAIL;
}

// Native exception test
// Tests a hardware-invoked invalid opcode error
// Inputs: none
// Outputs: FAIL on exit
// Side FX: Call IDT callback for invalid opcode exception.
int test_exception_invlopcode() {
	// Generates an undefined opcode, Intel manual reference vol2
	asm (
		"ud2"
	);
	return FAIL;
}

// Tests whether syscalls will work.
// Inputs: none
// Outputs: none
// Side effects: Should print Syscall! to the screen.
int idt_test_syscall() {
	TEST_HEADER;
	EMULATE_NORET_EXCEPTION(IDT_SYSCALL);
	return FAIL;
}

// Tests whether a NULL access pagefaults.
// Inputs: none
// Outputs: none
// Side effects: Should pagefault before anything is printed to screen.
int test_null_access_pf() {
	TEST_HEADER;
	int* a = NULL;
	printf("Value of a is %d\n", *a);
	return FAIL;
}

// Tests whether a VMEM access pagefaults.
// Inputs: none
// Outputs: none
// Side effects: Should NOT pagefault when reading VMEM value.
int test_vmem_no_pf() {
	TEST_HEADER;
	uint32_t* a = (uint32_t*)VIDMEM_KERN_BEGIN_ADDR;
	printf("Value of a is %d\n", *a);
	return PASS;
}
// add more tests here

void launch_tests_cp1() {

	TEST_OUTPUT("The first 20 IDT entries should have nonnull pointers", test_populated_idt());
	
	#ifdef CRASH_TESTING
	TEST_OUTPUT("Dereferencing NULL pagefaults", test_null_access_pf());
	TEST_OUTPUT("Division by zero should throw an exception.", test_exception_divzero());
	TEST_OUTPUT("Invalid opcode should throw an exception", test_exception_invlopcode());
	TEST_OUTPUT("Arbitrary exception handler works.", idt_exception_arbitrary());
	TEST_OUTPUT("Calling a syscall should trigger the IDT", idt_test_syscall());
	#endif

}
