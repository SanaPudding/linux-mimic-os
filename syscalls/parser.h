#ifndef PARSER_H
#define PARSER_H

#include "../types.h"
#include "../memfs/memfs.h"

#define EXEC_START_EIP_OFFSET 24
#define EXEC_MAGIC_NUMBYTES 4
#define EXEC_MAGIC_BYTE_1_OF_4 0x7f
#define EXEC_MAGIC_BYTE_2_OF_4 0x45
#define EXEC_MAGIC_BYTE_3_OF_4 0x4c
#define EXEC_MAGIC_BYTE_4_OF_4 0x46

typedef struct parse_command_result_t {
    // Index in the string where the command starts, inclusive
    uint32_t cmd_start_idx_incl;
    // Index in the string where the command ends, exclusive
    uint32_t cmd_end_idx_excl;
    // Index in the string where the arguments start, inclusive
    uint32_t args_start_idx;
    // Index in the string where the arguments end, exclusive
    uint32_t args_end_idx;
} parse_command_result_t;

parse_command_result_t parse_command(const char* input);

int extract_parsed_command(
            const char* input, 
            parse_command_result_t parsed_cmd_info, 
            char* to_fill, 
            uint32_t to_fill_size);

int extract_parsed_args(
            const char* input, 
            parse_command_result_t parsed_cmd_info, 
            char* to_fill, 
            uint32_t to_fill_size);

typedef struct executability_result_t {
    // Flag: 1 if the file is executable, 0 if not
    uint32_t is_executable;
    // Array to store the value of the start_eip
    uint8_t start_eip[4];
    uint32_t exec_inode;
    uint32_t exec_file_length;
} executability_result_t;

executability_result_t determine_executability(const char* filename);

uint32_t get_user_eip(executability_result_t exec_info);

int parse_command_result_t_COMPARE(const parse_command_result_t a, const parse_command_result_t b);

#endif
