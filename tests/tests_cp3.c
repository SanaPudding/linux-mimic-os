#include "tests.h"
#include "../syscalls/syscall_api.h"
#include "../syscalls/syscall.h"
#include "../device-drivers/keyboard.h" // Keyboard buf size wanted
#include "../syscalls/parser.h"
#include "../paging.h"
#include "../process/process.h"
#include "../memfs/fs_interface.h"

int test_syscall_idx_negativeone_fails() {
    int32_t retval;
    DO_SYSCALL_ZERO_ARGS(-1, retval);
    if (retval == -1) return PASS;
    else return FAIL;
}

int test_syscall_idx_ten_fails() {
    int32_t retval;
    DO_SYSCALL_ZERO_ARGS(10, retval);
    if (retval == -1) return PASS;
    else return FAIL;
}

int test_syscall_idx_zero_works() {
    int32_t retval;
    DO_SYSCALL_ZERO_ARGS(0, retval);
    if (retval != -1) return PASS;
    else return FAIL;
}

int test_syscall_idx_nine_works() {
    int32_t retval;
    DO_SYSCALL_ZERO_ARGS(9, retval);
    if (retval != -1) return PASS;
    else return FAIL;
}

int test_syscall_properly_returns_value() {
    if (sigreturn() == TEST_VALUE_xDEAD) return PASS;
    else return FAIL;
}

int test_syscall_execute_gets_command() {
    int32_t retval;
    DO_SYSCALL_ONE_ARG(SYSCALL_NUM_EXECUTE, retval, 0xF00F);
    if (retval == 0xBEEF) {
        printf("Extracted syscall correctly!!!\n");
        return PASS;
    } else if (retval == 0xDEAD) {
        printf("Couldn't extract argument in syscall!\n");
    } else {
        if (!retval) {
            printf("Got here from ripping IRET values, is this right?\n");
            return PASS;
        }
        else {
            printf("Unknown return val, something's amiss!\n");
            return FAIL;
        }
    }
    return PASS;
}

int test_parse_command_manycases() {
#define PARSE_CMD_TESTCASE(CMDSTR, EXPECT1, EXPECT2, EXPECT3 ) \
    { \
        const char* testcmd = CMDSTR ; \
        parse_command_result_t myresult = parse_command(testcmd); \
        parse_command_result_t expected = { EXPECT1, EXPECT2, EXPECT3 }; \
        if (parse_command_result_t_COMPARE(myresult, expected)) { \
            printf("Failed parsing: \"%s\"!\n", testcmd); \
            printf("Expected: {%d, %d, %d}\n", expected.cmd_start_idx_incl, expected.cmd_end_idx_excl, expected.args_start_idx); \
            printf("Got:      {%d, %d, %d}\n", myresult.cmd_start_idx_incl, myresult.cmd_end_idx_excl, myresult.args_start_idx); \
            return FAIL; \
        } \
    }

    PARSE_CMD_TESTCASE(0,                   -1, -1, -1);
    PARSE_CMD_TESTCASE("",                  0, 0, 0);
    PARSE_CMD_TESTCASE("             ",     13, 13, 13);
    PARSE_CMD_TESTCASE("hello",             0, 5, 5);
    PARSE_CMD_TESTCASE("        hello",     8, 13, 13);
    PARSE_CMD_TESTCASE("    hello    ",     4, 9, 13);
    PARSE_CMD_TESTCASE("hello -f -g -q",    0, 5, 6);
    PARSE_CMD_TESTCASE("hello       -q",    0, 5, 12);
    return PASS;
}

int test_dangerous_pagewalks() {

    uint32_t a; // test stack variable, should be fine
    (void)a;
    if (!(is_unsafe_page_walk((void*)0))) {
        printf("Null is safe!\n");
        return FAIL;
    }
    if (is_unsafe_page_walk(&a)) {
        printf("Stack variable is unsafe!\n");
        return FAIL;
    }
    if (is_unsafe_page_walk((void*)VIDMEM_KERN_BEGIN_ADDR)) {
        printf("Video memory is unsafe!\n");
        return FAIL;
    }
    if (!is_unsafe_page_walk((void*)(VIDMEM_KERN_BEGIN_ADDR + 4 * ONE_KB))) {
        printf("Off by one, outside vmem is safe!\n");
        return FAIL;
    }
    if (is_unsafe_page_walk((void*)(VIDMEM_KERN_BEGIN_ADDR + 4 * ONE_KB - 1))) {
        printf("Off by one, just in vmem is unsafe!");
        return FAIL;
    }
    return PASS;
}

int test_executability_manycases() {
#define EXECUTABILITY_TESTCASE(INPUT_CMD, SHOULD_EXECUTE) \
    { \
        char progname[KEYBOARD_BUF_SIZE]; /* Shell could NEVER read more than what we allocate for the keyboard */ \
        const char* fname = INPUT_CMD ; \
        executability_result_t determine_result; \
        parse_command_result_t parse_result = parse_command(fname); \
        if (extract_parsed_command(fname, parse_result, progname, KEYBOARD_BUF_SIZE)) { \
            printf("Unable to extract \"%s\"!", fname); \
            return FAIL; \
        } \
        printf("\tThe extracted program name is: \"%s\"\n", progname); \
        determine_result = determine_executability(progname); \
        printf("\t\"%s\" was determined to%s be executable!\n", \
                progname, \
                determine_result.is_executable ? "" : " not" ); \
        if (determine_result.is_executable != SHOULD_EXECUTE) { \
            return FAIL; \
        } \
    }

    EXECUTABILITY_TESTCASE("   .   ", 0);
    EXECUTABILITY_TESTCASE("   rtc   ", 0);
    EXECUTABILITY_TESTCASE("   cat   ", 1);
    EXECUTABILITY_TESTCASE("   counter   ", 1);
    EXECUTABILITY_TESTCASE("   fish   ", 1);
    EXECUTABILITY_TESTCASE("   frame0.txt   ", 0);
    EXECUTABILITY_TESTCASE("   frame1.txt   ", 0);
    EXECUTABILITY_TESTCASE("   grep   ", 1);
    EXECUTABILITY_TESTCASE("   grep  -r -c -z \"foobar\" ", 1);
    EXECUTABILITY_TESTCASE(" hello   ", 1);
    EXECUTABILITY_TESTCASE("ls   ", 1);
    EXECUTABILITY_TESTCASE("   shell", 1);
    EXECUTABILITY_TESTCASE("   sigtest ", 1);
    EXECUTABILITY_TESTCASE(" syserr ", 1);
    EXECUTABILITY_TESTCASE("   testprint", 1);
    EXECUTABILITY_TESTCASE(" verylargetextwithverylongname.tx   ", 0);
    EXECUTABILITY_TESTCASE(" doesntexist   ", 0);

    return PASS;
}

/*---------- syscall tests ----------*/

int test_syscall_terminal_write(const char* buf) {
    int32_t retval;
    int32_t fd = 1;
    int32_t nbytes = strlen(buf);
    DO_SYSCALL_THREE_ARGS(SYSCALL_NUM_WRITE, retval, fd, buf, nbytes);
    printf("retval: %d\n", retval);

    return PASS;
}

int test_syscall_terminal_read() {
    int32_t retval;
    int32_t nbytes = 10;
    uint8_t buf[nbytes];
    int i;
    for (i = 0; i < nbytes; i++) {
        buf[i] = '\0';
    }

    const char* write_string = "ECE391> ";
    int32_t write_nbytes = strlen(write_string);
    DO_SYSCALL_THREE_ARGS(SYSCALL_NUM_WRITE, retval, 1, write_string, write_nbytes);
    DO_SYSCALL_THREE_ARGS(SYSCALL_NUM_READ, retval, 0, buf, nbytes);
    printf("buf value: ");
    
    for (i = 0; i < nbytes; i++) {
        putc(buf[i]);
    }

    return PASS;
}

void test_rtc_ops() {
    int32_t retval;
    const char* filename = "rtc";
    int32_t rtc_fd;

    int32_t nbytes = 10;
    const unsigned char buf[10];

    DO_SYSCALL_ONE_ARG(SYSCALL_NUM_OPEN, rtc_fd, filename);         // open rtc
    printf("rtc opened\n");
    printf("rtc_fd: %d\n", rtc_fd);

    DO_SYSCALL_THREE_ARGS(SYSCALL_NUM_READ, retval, 0, buf, nbytes);        // pause
    DO_SYSCALL_THREE_ARGS(SYSCALL_NUM_WRITE, retval, rtc_fd, buf, 1024);    // write rtc
    printf("try write rtc -- 1024\n");

    DO_SYSCALL_THREE_ARGS(SYSCALL_NUM_READ, retval, 0, buf, nbytes);     // pause
    DO_SYSCALL_THREE_ARGS(SYSCALL_NUM_WRITE, retval, rtc_fd, buf, 2);    // write rtc
    printf("try write rtc -- 2\n");

    DO_SYSCALL_THREE_ARGS(SYSCALL_NUM_READ, retval, 0, buf, nbytes);        // pause
    DO_SYSCALL_ONE_ARG(SYSCALL_NUM_CLOSE, retval, rtc_fd);                  // close rtc
    printf("rtc closed\n");

    DO_SYSCALL_THREE_ARGS(SYSCALL_NUM_READ, retval, 0, buf, nbytes);     // pause
    DO_SYSCALL_THREE_ARGS(SYSCALL_NUM_WRITE, retval, rtc_fd, buf, 1024);    // write rtc
    printf("try write rtc -- 1024\n");
}

int test_open_multiple_fds() {
    int32_t i;
    int32_t ret_fds[8];
    const char* filename = "rtc";
    for (i = 0; i < 8; i++) {
        DO_SYSCALL_ONE_ARG(SYSCALL_NUM_OPEN, ret_fds[i], filename);
    }
    for (i = 0; i < 6; i++) {
        if (ret_fds[i] != (i+2)) {return FAIL;}
    }
    if (ret_fds[6] != -1) {return FAIL;}
    if (ret_fds[7] != -1) {return FAIL;}
    return PASS;
}

int test_close_multiple_fds() {
    int32_t i;
    int32_t ret_vals[8];
    for (i = 0; i < 8; i++) {
        DO_SYSCALL_ONE_ARG(SYSCALL_NUM_CLOSE, ret_vals[i], i);
    }
    if (ret_vals[0] != -1) {return FAIL;}
    if (ret_vals[1] != -1) {return FAIL;}
    for (i = 2; i < 8; i++) {
        if (ret_vals[i]) {return FAIL;}
    }
    return PASS;
}

int test_reading_directory_through_syscall() {
    int32_t fd;
    const char* filename = ".";
    DO_SYSCALL_ONE_ARG(SYSCALL_NUM_OPEN, fd, filename);
	if (fd == -1) {
		printf("Couldn't open directory!\n");
		return FAIL;
	}
	printf("Opened directory.\n");

	char buf[16];
	int32_t retval;
	int32_t files_read = 0;

    while (1) {
        DO_SYSCALL_THREE_ARGS(SYSCALL_NUM_READ, retval, fd, buf, 15);
        if (retval == -1) {
			break;
        } else {
            buf[15] = 0;
            #ifdef PRINT_TESTING
			printf("Item: %s\n", buf);
            #endif
			files_read++;
        }
    }

    DO_SYSCALL_ONE_ARG(SYSCALL_NUM_CLOSE, retval, fd);
	if (retval == -1) {
		printf("Problem closing opened directory!\n");
		return FAIL;
	}

	if (files_read != fs_boot_blk_location->dentry_count) {
		printf("Mismatch in total count! Wanted to read %d dentries, but read %d.\n", 
			fs_boot_blk_location->dentry_count, files_read);
		return FAIL;
	}
	return PASS;
}

int test_reading_textfile_through_syscall() {
	int32_t fd;
    const char* filename = "frame0.txt";
    DO_SYSCALL_ONE_ARG(SYSCALL_NUM_OPEN, fd, filename);
	if (fd == -1) {
		printf("Couldn't read text file!\n");
		return FAIL;
	}
	printf("Opened text file.\n");
    
    #ifdef PRINT_TESTING
	char buf = 0;
	int32_t retval;
    int32_t test_result = 0;
    while (1) {
        DO_SYSCALL_THREE_ARGS(SYSCALL_NUM_READ, retval, fd, &buf, 1);
        if (retval == -1) {
            test_result = -1;
            break;
        } else if (retval == 0) {
            printf("reads in 0 byte\n");
            break;
        } else {
            putc(buf);
        }
    }
    #endif

    DO_SYSCALL_ONE_ARG(SYSCALL_NUM_CLOSE, retval, fd);
    if (retval == -1) {
        printf("Couldn't close!\n");
        return FAIL;
    }
    if (test_result == -1) {
        return FAIL;
    }
    return PASS;
}

int test_read_from_empty_fd() {
    char buf = 0;
	int32_t retval;
    DO_SYSCALL_ONE_ARG(SYSCALL_NUM_CLOSE, retval, 7);
    DO_SYSCALL_THREE_ARGS(SYSCALL_NUM_READ, retval, 7, &buf, 1);
    if (retval == -1) return PASS;
    else return FAIL;
}

int test_write_to_empty_fd() {
    char buf[50] = "test string";
	int32_t retval;
    DO_SYSCALL_ONE_ARG(SYSCALL_NUM_CLOSE, retval, 7);
    DO_SYSCALL_THREE_ARGS(SYSCALL_NUM_WRITE, retval, 7, &buf, 11);
    if (retval == -1) return PASS;
    else return FAIL;
}


/*---------- syscall test end ----------*/

void launch_tests_cp3() {
    TEST_OUTPUT("Syscall number -1  fails", test_syscall_idx_negativeone_fails());
    TEST_OUTPUT("Syscall number 10  fails", test_syscall_idx_ten_fails());
    TEST_OUTPUT("Syscall number  0 passes", test_syscall_idx_zero_works());
    TEST_OUTPUT("Syscall number  9 passes", test_syscall_idx_nine_works());
    #ifdef SYSCALL_RET_TESTING
    TEST_OUTPUT("Obtain return value from syscall", test_syscall_properly_returns_value());
    #endif
    #ifdef SYS_EXECUTE_TEST
    TEST_OUTPUT("Sysexecute can read command variable", test_syscall_execute_gets_command());
    #endif
    TEST_OUTPUT("Parsing commands works as expected", test_parse_command_manycases());
    TEST_OUTPUT("Programs are properly determined to be executable", test_executability_manycases());
    TEST_OUTPUT("test_dangerous_pagewalks", test_dangerous_pagewalks());

    process_init();
    if (!process_allocate(NO_PARENT_PID)) {
        printf("pcb allocation failed\n");
        return;
    }

    
    TEST_OUTPUT("Test OPENING multiple file descriptors", test_open_multiple_fds());
    TEST_OUTPUT("Test CLOSING multiple file descriptors", test_close_multiple_fds());
    TEST_OUTPUT("Test reading directory", test_reading_directory_through_syscall());
    TEST_OUTPUT("Test reading textfile", test_reading_textfile_through_syscall());
    TEST_OUTPUT("Test reading empty file descriptor", test_read_from_empty_fd());
    TEST_OUTPUT("Test writing empty file descriptor", test_write_to_empty_fd());
    // test_getargs();

//     test_rtc_ops();

// #ifdef TEST_TERMINAL_SYSCALL
//     process_init();
//     if (!process_allocate()) {
//         printf("pcb allocation failed\n");
//         return;
//     }

// /*----------------------------------------*/
// // sec 1
//     test_syscall_terminal_write("Test sys write 1: haha> ");
//     test_syscall_terminal_write("Test sys write 2: ha> ");
//     test_syscall_terminal_write("Test sys write 3: > ");

//     while (1) {
//         test_syscall_terminal_read();
//     }
/*----------------------------------------*/
// sec 2
// #endif
}
