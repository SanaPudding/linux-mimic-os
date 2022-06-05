#include "../common.h"
#include "../lib.h"
#include "tests.h"
#include "../device-drivers/keyboard.h"
#include "../device-drivers/terminal.h"
#include "../device-drivers/rtc.h"
#include "../memfs/memfs.h"



/* Checkpoint 2 tests */

/* terminal & keyboard test */

// test terminal_read when the requested number of bytes
// to be read is smaller than the actually number of bytes
// typed in.
int test_terminal_read_underflow() {
	int i;
	int8_t buf[20];
	for (i = 0; i < 20; i++) {
		buf[i] = '\0';
	}
	int ret_val;

	TEST_HEADER;
	printf("keyboard input: ");
	ret_val = terminal_read(NULL, (void*) buf, 20);
	printf("buffer output: ");
	for (i = 0; i < 20; i++) {
		putc(buf[i]);
	}
	printf("bytes read: %d\n", ret_val);
	return PASS;
}

// test terminal_read when the requested number of bytes
// to be read is greater than the actually number of bytes
// typed in.
int test_terminal_read_overflow() {
	int i;
	int8_t buf[200];
	for (i = 0; i < 200; i++) {
		buf[i] = '\0';
	}
	int ret_val;

	TEST_HEADER;
	printf("keyboard input: ");
	ret_val = terminal_read(NULL, (void*) buf, 200);
	printf("buffer output: ");
	for (i = 0; i < 200; i++) {
		putc(buf[i]);
	}
	printf("bytes read: %d\n", ret_val);
	return PASS;
}

// test terminal_write
int test_terminal_write(uint8_t* buf, int n) {
	int ret_val;

	TEST_HEADER;
	printf("terminal write output: ");
	ret_val = terminal_write(0, (void*) buf, n);
	printf("\nbytes write: %d\n", ret_val);
	return (ret_val == n) ? PASS : FAIL;
}

/* RTC test */

// tests invalid write , not power of two
int test_rtc_invalid_write_powertwo(){
	TEST_HEADER;
	int32_t buf[1];
	
	buf[0] = 0x3;
	
	int retval = rtc_write(0, (void*)buf, 4);

	if(retval==-1){
		printf("FAIL\n");
		return FAIL;
	}
	return PASS;
}

// tests invalid write, frequency larger than 1024hz
int test_rtc_invalid_write_largernum(){
	TEST_HEADER;
	int32_t buf[1];
	
	//2048hz as frequency
	buf[0] = 0x0800;
	
	int retval = rtc_write(0, (void*)buf, 4);

	if(retval==-1){
		printf("FAIL\n");
		return FAIL;
	}
	return PASS;
}

// tests if rtc writes work
int test_rtc_write(){
	TEST_HEADER;

	int32_t buf[1];
	//starts at 1024
	buf[0] = 0x0400;
	float x = 0;
	//loops until frequency gets to 2
	while(buf[0] >= 2){
		int retval = rtc_write(0, (void*)buf, 4);
		if (retval == -1) return FAIL;
		while(x < 5000000){
			x+= 1;
		}
		x = 0;
		buf[0] = buf[0] >> 1;
		
	}

	
	return PASS;
}

// tests if rtc_open sets the frequency to 2hz. 
int test_rtc_open(){
	TEST_HEADER;
	float x = 0;

	rtc_open();
	// wait for a short period of time 

	while(x < 10000000){
			x += 1;
	}
	return PASS;
}

//tests if rtc_close returns 0
int test_rtc_close(){
	TEST_HEADER;
	if(rtc_close() == 0){
		return PASS;	
	}else{
		printf("FAIL\n");
		return FAIL;
	}
}

// tests if interrupt happens before return
int test_rtc_readafterinq(){
	TEST_HEADER;
	int32_t buf[1];
	buf[0] = 0x0002;
	printf("return %d", rtc_read(0, (void*)buf, 4));
	
	return PASS;
	
	
}




// I used a hexdump to inspect filesys_img, if this was mounted right we should read
// the values 0x11000000 40000000 from the filesystem.
// Inputs: None
// Outputs: None
// Side-effects: Accesses fs_boot_blk_location (listed here because page faults count as a side effect).
int test_filesystem_location_start_correct() {
	TEST_HEADER;
	uint32_t* firstfourbytes = (uint32_t*)(fs_boot_blk_location);
	uint32_t* secondfourbytes = firstfourbytes+1;
	if (*firstfourbytes == 0x11000000 && *secondfourbytes == 0x40000000) return FAIL;
	else return PASS;
}

// Tests whether we can find a dentry by a dentry index.
// Inputs: None
// Outputs: None
// Side effect: N/A
int test_read_dentry_by_index() {
	fs_boot_blk_dentry_t sigtest_dentry;
	int32_t retval = read_dentry_by_index(1, &sigtest_dentry);
	if (retval == -1) return FAIL;
	else {
		if (sigtest_dentry.filename[0] != 's') return FAIL;
		if (sigtest_dentry.filename[1] != 'i') return FAIL;
		if (sigtest_dentry.filename[2] != 'g') return FAIL;
		else return PASS;
	}
}

// Reading a dentry by a name that is longer than 32 bytes should fail.
// Inputs, Outputs, Side effects: None
int test_read_dentry_by_name_reallylong_fullname() {
	fs_boot_blk_dentry_t long_dentry;
	const char* fn = "verylargetextwithverylongname.txt";
	int32_t retval = read_dentry_by_name(fn, &long_dentry);
	if (retval == -1) return PASS;
	else return FAIL;
}

// Reading a dentry by the truncated name that is 32 bytes should pass.
// Inputs, Outputs, Side effects: None
int test_read_dentry_by_name_reallylong_truncname() {
	fs_boot_blk_dentry_t long_dentry;
	const char* fn = "verylargetextwithverylongname.tx";
	int32_t retval = read_dentry_by_name(fn, &long_dentry);
	if (retval == -1) return FAIL;
	else return PASS;
}

// We should be able to find all of the files in the filesystem by name, including the directory.
// Inputs, Outputs, Side effects: None
int test_finding_all_dentries() {
	fs_boot_blk_dentry_t d;
	const char* files[32] = {
		".",
		"sigtest",
		"shell",
		"grep",
		"syserr",
		"rtc",
		"fish",
		"counter",
		"pingpong",
		"cat",
		"frame0.txt",
		"verylargetextwithverylongname.tx",
		"ls",
		"testprint",
		"created.txt",
		"frame1.txt",
		"hello"
	};
	uint8_t i = 0;
	for (i = 0; i < 17; i++) {
		int32_t retval = read_dentry_by_name(files[i], &d);
		if (retval == -1) return FAIL;
	}
	return PASS;
}

// We should be able to read a small number of bytes from frame0.txt
// Inputs, Outputs, Side effects: None
int test_read_data_from_frame0_txt_four_bytes() {
	fs_boot_blk_dentry_t frame1_dentry;
	read_dentry_by_name("frame0.txt", &frame1_dentry);
	char buf[5];
	int32_t bytes_read = read_data(frame1_dentry.inode_idx, 0, (uint8_t*)buf, 4);
	if (bytes_read != 4) {
		printf("Only read %u bytes...\n", bytes_read);
		return FAIL;
	}
	if (buf[0] != '/' || buf[1] != '\\' || buf[2] != '/' || buf[3] != '\\') {
		printf("Expected /\\/\\, got %s\n", buf);
		return FAIL;
	}
	return PASS;
}

// We should be able to read all the data from frame0.txt
// Inputs, Outputs, Side effects: None
int test_read_data_from_frame0_txt_allbytes() {
	fs_boot_blk_dentry_t frame1_dentry;
	read_dentry_by_name("frame0.txt", &frame1_dentry);
	char buf[450];
	uint32_t total_bytes = ith_inode_blk(frame1_dentry.inode_idx)->len_in_bytes;
	int32_t bytes_read = read_data(frame1_dentry.inode_idx, 0, (uint8_t*)buf, total_bytes);
	if (bytes_read != total_bytes) {
		printf("Only read %u bytes...\n", bytes_read);
		return FAIL;
	}
	#ifdef PRINT_TESTING
	uint32_t i;
	printf("Printing what was read, it should look like a fish.\n");
	for (i = 0; i < total_bytes; i++) {
		putc(buf[i]);
	}
	#endif
	return PASS;
}

// We should be able to read all the data from a very long file.
// Inputs, Outputs, Side effects: None
int test_read_data_from_verylongfile() { 
	fs_boot_blk_dentry_t long_dentry;
	read_dentry_by_name("verylargetextwithverylongname.tx", &long_dentry);
	char buf[5278];
	uint32_t total_bytes = ith_inode_blk(long_dentry.inode_idx)->len_in_bytes;
	int32_t bytes_read = read_data(long_dentry.inode_idx, 0, (uint8_t*)buf, total_bytes);
	if (bytes_read != total_bytes) {
		printf("Only read %u bytes...\n", bytes_read);
		return FAIL;
	}
	#ifdef PRINT_TESTING
	uint32_t i;
	printf("Printing what was read, it should look like increasing ascii chars.\n");
	for (i = 0; i < total_bytes; i++) {
		putc(buf[i]);
	}
	#endif
	return PASS;
}

// We should be able to open, read, and close a directory using the fake_fs API
// Inputs, outputs, side effects: None

/*---------- discarded test functions ----------*/
#ifdef DISCARD
int test_reading_directory_using_fakeapi() {
	int32_t fd = fake_fs_open(".");
	if (fd == -1) {
		printf("Couldn't open directory!\n");
		return FAIL;
	}
	printf("Opened directory.\n");
	uint8_t files_read = 0;
	char buf[16];
	int32_t retval;
	while ((retval = fake_fs_read(fd, (uint8_t*)buf, 15)) != 0) {
		if (retval == -1) {
			printf("Couldn't read directory entry #%d!\n", files_read);
			return FAIL;
		} else {
			buf[15] = 0;
            #ifdef PRINT_TESTING
			printf("Item: %s\n", buf);
            #endif
			files_read++;
		}
	}
	retval = fake_fs_close(fd);
	if (retval == -1) {
		printf("Problem closing opened directory!\n");
		return FAIL;
	}

	if (files_read != fs_boot_blk_location->dentry_count) {
		printf("Mismatch in total count! Wanted to read %d dentries, but read %d.\n", 
			fs_boot_blk_location->dentry_count, files_read);
		return FAIL;
	}
	printf("Pass.\n");
	return PASS;
}

// We should be able to open, read, and close a text file using the fake_fs API
// Inputs, outputs, side effects: None
int test_reading_textfile_using_fakeapi() {
	int32_t fd = fake_fs_open("frame0.txt");
	if (fd == -1) {
		printf("Couldn't read text file!\n");
		return FAIL;
	}
	printf("Opened text file.\n");
    #ifdef PRINT_TESTING
	char buf = 0;
	int32_t retval;
	while ((retval = fake_fs_read(fd, (uint8_t*)&buf, 1)) != 0) {
		putc(buf);
	}
    printf("If it looks like a fish, we passed.\n");
    #endif
	if (fake_fs_close(fd) == -1) {
        printf("Couldn't close!\n");
        return FAIL;
    };
	return PASS;
}

// We should be able to open, read, and close a binary file using the fake_fs API
// Inputs, outputs, side effects: None
int test_reading_binary_file_using_fakeapi() {
	int32_t fd = fake_fs_open("hello");
	if (fd == -1) {
		printf("Couldn't read binary file!\n");
		return FAIL;
	}
    #ifdef PRINT_TESTING
	char buf;
	int32_t retval;
	while ((retval = fake_fs_read(fd, (uint8_t*)&buf, 1)) != 0) {
		putc(buf);
	}
    printf("\nWe should see magic at the end. If we do, we passed.\n");
    #endif
	if (fake_fs_close(fd) == -1) {
        printf("Couldn't close!\n");
        return FAIL;
    };
	return PASS;
}

// Reading an unopened file fails
// Inputs, outputs, side effects: None
int test_reading_unopened_file_using_fakeapi() {
    char buf[3];
    if (fake_fs_read(3, (uint8_t*)buf, 2) != -1) return FAIL;
    return PASS;
}

// Closing an unopened file fails
// Inputs, outputs, side effects: None
int test_closing_unopened_file_using_fakeapi() {
    if (fake_fs_close(3) != -1) return FAIL;
    return PASS;
}

// Trying to write to the filesystem fails ALWAYS
// Inputs, outputs, side effects: None
int test_writing_to_filesystem_fails_using_fakeapi() {
	char buf[16] = {'E', 'C', 'E', ' ', '3', '9', '1', ' ', 'i', 's', ' ', 'h', 'a', 'r', 'd', 0};
	int32_t fd = fake_fs_open("grep");
	if (fake_fs_write(fd, (uint8_t*)buf, 15) != -1) {
		printf("You are bad\n");
		return FAIL;
	};
	fake_fs_close(fd);
	return PASS;
}
#endif

void launch_tests_cp2() {    
    TEST_GROUP("FS Dentries",
	    TEST_IN_GROUP("FS dentries are successfully read by index", test_read_dentry_by_index())
	    TEST_IN_GROUP("FS dentries aren't read with too-long name", test_read_dentry_by_name_reallylong_fullname())
	    TEST_IN_GROUP("FS dentries are read with max-length name", test_read_dentry_by_name_reallylong_truncname())
	    TEST_IN_GROUP("We can find all the dentries", test_finding_all_dentries())
    );
    TEST_GROUP("FS Reading Data", 
        TEST_IN_GROUP("We can read four bytes from frame0.txt", test_read_data_from_frame0_txt_four_bytes())
        TEST_IN_GROUP("We can read all bytes from frame0.txt", test_read_data_from_frame0_txt_allbytes())
    );
    // TEST_GROUP("Filesystem fake read/write/open/close",
    //     TEST_IN_GROUP("We can read all bytes from a very long file", test_read_data_from_verylongfile())
	//     TEST_IN_GROUP("Reading directory via fake API", test_reading_directory_using_fakeapi())
	//     TEST_IN_GROUP("Reading text file via fake API", test_reading_textfile_using_fakeapi())
    //     TEST_IN_GROUP("Reading binary file via fake API", test_reading_binary_file_using_fakeapi())
    //     TEST_IN_GROUP("Reading unopened file fails", test_reading_unopened_file_using_fakeapi())
    //     TEST_IN_GROUP("Closing unopened file fails", test_closing_unopened_file_using_fakeapi())
	// 	TEST_IN_GROUP("Writing to anything always fails", test_closing_unopened_file_using_fakeapi())
    // );
    TEST_OUTPUT("FS location properly retrieved", test_filesystem_location_start_correct());

	
	// test_rtc_open();
	// test_rtc_close();
	// test_rtc_write();
	// test_rtc_invalid_write_powertwo();
	// test_rtc_invalid_write_largernum();
	// test_rtc_readafterinq();
	
	

    // while (1) {
	// 	// TEST_OUTPUT("ternimal read underflow", test_terminal_read(20))
	// 	// TEST_OUTPUT("ternimal read overflow", test_terminal_read(200))
	// 	// TEST_OUTPUT("ternimal write underflow", test_terminal_write("\0\0\0\0\0hello-this-is-terminal-write-test", 10))
	// 	test_terminal_read_underflow();
	// 	test_terminal_read_overflow();
	// 	test_terminal_write((uint8_t*)"\0\0\0\0\0hello-this-is-terminal-write-test", 20);
	// }
	
}
