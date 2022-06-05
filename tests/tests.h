#ifndef TESTS_H
#define TESTS_H

#include "../types.h"
#include "../lib.h"

// test launcher
void launch_tests();
void launch_tests_cp1();
void launch_tests_cp2();
void launch_tests_cp3();
void launch_tests_cp4();
void launch_tests_cp5();

// ------------------CONFIGURATION
#define IS_TESTING // Controls whether tests are run
// #define CRASH_TESTING // Controls whether tests that crash are run
#define PRINT_TESTING // Controls whether tests that print a lot are run
// #define RTC_TESTING //macro to only print things when RTC functionality is being tested.
// #define SYSCALL_RET_TESTING // DO NOT USE
// #define SYS_EXECUTE_TEST // DO NOT USE
// -------------------------------
#define TEST_SPIN() while (1){}
#define TEST_VALUE_xECEB 0xECEB
#define TEST_VALUE_xDEAD 0xDEAD
#define TEST_VALUE_xBEEF 0xBEEF
#define TEST_VALUE_xFEED 0xFEED 


/* format these macros as you see fit */
#define TEST_HEADER 	\
	printf("[TEST %s] %s:%d\n", __FUNCTION__, __FILE__, __LINE__)

#define TEST_OUTPUT(name, result)	\
	{\
		int res = result;\
		printf("[TEST %s] Result = %s\n", name, (res) ? "PASS" : "FAIL"); if (!res) {while (1) {}}\
	}

#define TEST_GROUP(name, tests...) \
    {\
        printf("[GROUP %s] %s:%d\n", name, __FILE__, __LINE__);\
        int total = 0;\
        int passing = 0;\
        tests ; \
        int fullpass = (passing < total) ? FAIL : PASS;\
        printf("[GROUP %s] Result = %s, %d/%d passed\n", name, fullpass ? "PASS" : "FAIL", passing, total);\
        if (!fullpass) while(1){}\
    }

#define TEST_IN_GROUP(name, result) \
    {\
        int res = result;\
        total += 1;\
        if (res == PASS) passing += 1;\
        printf("----[SUBTEST: %s] Result = %s\n", name, (res) ? "PASS" : "FAIL");\
    } 

#define PASS 1
#define FAIL 0

#define EMULATE_NORET_EXCEPTION(NUM) asm volatile ("int %0;" : : "i" (NUM) : "flags" );
#define EMULATE_RET_EXCEPTION(NUM, RETCODE) asm volatile ("pushl %0; int %1;" : : "i" ( RETCODE ), "i" ( NUM ) : "memory" );

#endif /* TESTS_H */
