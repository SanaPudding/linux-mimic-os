#include "parser.h"

// Determines struct equality between two parse_command_result_ts
// Inputs: 
//      a: parse_command_result_t 
//      b: parse_command_result_t 
// Output: -1 if not equal, 0 if equal
// Side FX: None
int parse_command_result_t_COMPARE(const parse_command_result_t a, const parse_command_result_t b) {
    if (    a.cmd_start_idx_incl != b.cmd_start_idx_incl
        ||  a.cmd_end_idx_excl != b.cmd_end_idx_excl
        ||  a.args_start_idx != b.args_start_idx ) return -1;
    else return 0;
}

// Returns the result of parsing an input string
// Inputs:
//      input: Pointer to null-terminated input string
// Outputs: The parse result
// Side FX: none
parse_command_result_t parse_command(const char* input) {
    parse_command_result_t result = { 
        .cmd_start_idx_incl = 0, 
        .cmd_end_idx_excl = 0,
        .args_start_idx = 0
    };

    // Null pointer passed, mark as total fail by setting all fields to -1
    // Useful consequence - length checking this still results in zero
    if (!input) {
        result.cmd_start_idx_incl = -1;   
        result.cmd_end_idx_excl = -1;
        result.args_start_idx = -1;
        result.args_end_idx = -1;
        return result;
    }
    
    // Increment until we don't encounter a space
    uint32_t start = 0;
    while (input[start] == ' ') {
        start++;
    }
    
    // If we encountered null, there is no command
    if (input[start] == '\0') {
        result.cmd_start_idx_incl = start;   
        result.cmd_end_idx_excl = start;
        result.args_start_idx = start;
        return result;
    }

    result.cmd_start_idx_incl = start;
    
    // Because end is exclusive, we must start at `1+start`
    // Continue scanning until we hit null, or until we hit a space
    uint32_t end = start+1;
    while (input[end] && input[end] != ' ') {
        end++;
    }
    result.cmd_end_idx_excl = end;
    
    // Where the arguments start is where we stop reading spaces (could be nullbyte)
    uint32_t args_start = end;
    while (input[args_start] == ' ') {
        args_start++;
    }
    // Postcondition: input[args_start] is not space (could be null, could be data)
    result.args_start_idx = args_start;
    
    uint32_t args_end = args_start;

    while(input[args_end] != '\0'){
        args_end++;
    }
    
    result.args_end_idx = args_end;

    return result;
}

// Given an input string and the result of its parsing, fill the to_fill buffer with the name of the command
// Inputs:
//      input: pointer to input string
//      parsed_cmd_info: parse result from parse_command
//      to_fill: buffer to fill the command with
//      to_fill_size: Size of to_fill_buffer
// Returns:
//      -1 on invalid parameters
//      Otherwise # of bytes unable to be written (0 is always success)
// Side effects: Fills the to_fill buffer
int extract_parsed_command(const char* input, parse_command_result_t parsed_cmd_info, char* to_fill, uint32_t to_fill_size) {
    if (parsed_cmd_info.cmd_start_idx_incl == -1 || !input || !to_fill) return -1;
    else {
        // The remaining number of bytes we need to fill is here
        uint32_t remaining = parsed_cmd_info.cmd_end_idx_excl - parsed_cmd_info.cmd_start_idx_incl;
        uint32_t start_pos = parsed_cmd_info.cmd_start_idx_incl;
        uint32_t curpos = 0;
        // As we go through and fill the buffer, we deplete our to_fill_size
        // Once it's zero, we have no more space to fill memory
        while (remaining && to_fill_size > 0) {
            to_fill[curpos] = input[start_pos + curpos];
            remaining--;
            to_fill_size--;
            curpos++;
        }
        // Do we still have room? Fill the null byte if so
        if (to_fill_size != 0) {
            to_fill[curpos] = 0;
            to_fill_size--; 
        } else {
            // Need to convey that we haven't filled in the null byte!
            remaining += 1;
        }
        return remaining;
    }
}

// Given an input string and the result of its parsing, fill the to_fill buffer with the name of the command
// Inputs:
//      input: pointer to input string
//      parsed_cmd_info: parse result from parse_command
//      to_fill: buffer to fill the arguments with
//      to_fill_size: Size of to_fill_buffer
// Returns:
//      -1 on invalid parameters
//      Otherwise # of bytes unable to be written (0 is always success)
// Side effects: Fills the to_fill buffer
int extract_parsed_args(const char* input, parse_command_result_t parsed_cmd_info, char* to_fill, uint32_t to_fill_size) {
    if (parsed_cmd_info.args_start_idx == -1 || !input || !to_fill) return -1;
    else {
        uint32_t remaining = parsed_cmd_info.args_end_idx - parsed_cmd_info.args_start_idx;
        uint32_t start_pos = parsed_cmd_info.args_start_idx;
        uint32_t curpos = 0;
        while (remaining && to_fill_size > 0) {
            to_fill[curpos] = input[start_pos + curpos];
            remaining--;
            to_fill_size--;
            curpos++;
        }
        if (to_fill_size != 0) {
            to_fill[curpos] = 0; // Null byte
            to_fill_size--; 
        } else {
            // Need to convey that we haven't filled in the null byte!
            remaining += 1;
        }
        return remaining;
    }
}

// Function to determine the executability, given a string holding the name of the command/file to run
// Inputs:
//      filename: pointer to a string holding the executable name
// Outputs: Executability result
// Side effects: None
executability_result_t determine_executability(const char* filename) {
    executability_result_t res = { .is_executable = 0 };
    fs_boot_blk_dentry_t fdentry;

    // Make sure the file exists and is a *file*
    if (!filename || read_dentry_by_name(filename, &fdentry) == -1) {
        return res;
    }
    if (fdentry.filetype != FS_TYPE_FILE) return res;
    
    // Try to read the magic bytes, fail if there isn't enough data in the file or if the magic
    // doesn't exist.
    uint8_t magic_buffer[EXEC_MAGIC_NUMBYTES];
    if (read_data(fdentry.inode_idx, 0, magic_buffer, EXEC_MAGIC_NUMBYTES) != EXEC_MAGIC_NUMBYTES)
        return res;
    
    if (!(  magic_buffer[0] == EXEC_MAGIC_BYTE_1_OF_4
        &&  magic_buffer[1] == EXEC_MAGIC_BYTE_2_OF_4
        &&  magic_buffer[2] == EXEC_MAGIC_BYTE_3_OF_4
        &&  magic_buffer[3] == EXEC_MAGIC_BYTE_4_OF_4 )) return res;
    
    // Try to read the data up to where EIP is, if we can't even read EIP
    // (because the file is just not long enough), it can't possibly be executable.
    if (read_data(fdentry.inode_idx, EXEC_START_EIP_OFFSET, res.start_eip, sizeof(uint32_t)) != sizeof(uint32_t))
        return res;

    // Note, there used to be code to reverse EIP, but that would be necessary if the file was
    // big endian...if we're directly mapping this into user memory, and it was *actually* big
    // endian, then we would have to reverse the bytes for every single opcode we had
    // AND the bytes for the memory

    res.exec_inode = fdentry.inode_idx;
    res.exec_file_length = ith_inode_blk(fdentry.inode_idx)->len_in_bytes;
    // Mark as executable
    res.is_executable = 1;
    return res;
}

// Returns the user's starting EIP
// Inputs:
//      exec_info: executability_result_t from determine_executability
// Outputs: Value of user EIP
// Side effects: None
uint32_t get_user_eip(executability_result_t exec_info) {
    return *(uint32_t*)(exec_info.start_eip);
}
