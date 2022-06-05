#include "tests.h"
#include "../lib.h"

/* Test suite entry point */
#ifdef IS_TESTING
void launch_tests(){
	reset_cursor();
	clear();
	printf("Clearing output for tests!\n");
	launch_tests_cp3();
	// launch_tests_cp1();	
	// launch_tests_cp2();
	// launch_tests_cp4();
	// launch_tests_cp5();
	
    #ifndef PRINT_TESTING
    printf("/!\\ Print testing was disabled.\n");
    #endif
    #ifndef CRASH_TESTING
    printf("/!\\ Crash testing was disabled.\n");
    #endif

}
#else 
void launch_tests() {}
#endif
