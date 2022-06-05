#include "types.h"
#ifndef COMMON_H
#define COMMON_H

#ifndef ASM

// https://gcc.gnu.org/onlinedocs/gcc/Zero-Length.html
// https://gcc.gnu.org/onlinedocs/gcc-4.1.2/gcc/Empty-Structures.html#Empty-Structures
// Static assert mechanism that takes advantage of the GCC c-extensions, so no memory (besides a pointer due to the symbol) is actually allocated.
// Just use the STATIC_ASSERT macro, see CEILDIV for example usage.
typedef struct {} dummy_struct;
#define _INDIRECT_MACRO2_STATIC_ASSERT(cond, line) static dummy_struct CompileAssertionFailureOnLine ## line[(cond) ? 0 : -1] __attribute__ ((unused));
#define _INDIRECT_MACRO1_STATIC_ASSERT(cond, line) _INDIRECT_MACRO2_STATIC_ASSERT(cond, line)
#define STATIC_ASSERT(cond) _INDIRECT_MACRO1_STATIC_ASSERT(cond, __LINE__ )

// Calculates the ceiling division between two integral types. Learned this in ECE 408.
#define CEILDIV(a, b) ((a + b - 1) / b)

// Sanity tests for ceiling division.
STATIC_ASSERT(CEILDIV(3, 3) == 1);
STATIC_ASSERT(CEILDIV(4, 3) == 2);
STATIC_ASSERT(CEILDIV(0, 3) == 0);

#define ONE_MB 0x100000
#define ONE_KB 0x400

// Inputs:
//      condition: A boolean condition to check a condition
//      ...: Arguments you would like to pass to printf (could be empty, could consist of a string, OR could be a string along with variables)
// Outputs: None
// Side effects: Prints to the screen the file, function, and line in the file where the assertion failed, and busy loops
// Examples: 
//      PRINT_ASSERT(1 == 1); 
//      PRINT_ASSERT(2 == 1, "Why isn't 2 == 1?"); 
//      PRINT_ASSERT(is_invalid(value), "Value %d was unexpected!\n", value);

#define PRINT_ASSERT(condition, ...) do { asm("cli"::); if (!( condition )) { printf("Assertion \"%s\" failed in function \"%s:%s:%d\"!\n", #condition, __FILE__, __FUNCTION__, __LINE__ );\
        printf( "" __VA_ARGS__ );\
        while(1){}\
    }\
} while(0);

static inline int _macro_cli_save_and_flags_begin(uint32_t* flags) {
    uint32_t f;
    asm ( 
        "pushfl;" 
        "popl %%eax;" 
        "cli;"  
        "movl %%eax, %[fls];" 
        : [fls] "=m" (f)
        : : "eax" 
    );
    *flags = f;
    return 0;
}

static inline int _macro_cli_save_and_flags_end(uint32_t* flags) {
    uint32_t f = *flags;
    asm( 
        "movl %[fls], %%eax;" 
        "pushl %%eax;" 
        "popfl;" 
        :
        : [fls] "m" (f) 
        : "eax" 
    );
    return 1;
}


// Usage:
// uint32_t flags, garbage;
// CRITICAL_SECTION_FLAGSAVE(flags, garbage) {
//      Your sharemem code here...
// }
// Do NOT put a return statement in the block, put it outside the block! You might otherwise forget to restore the proper flags!
#define CRITICAL_SECTION_FLAGSAVE(flags, __cli_mark) \
    for ( __cli_mark = _macro_cli_save_and_flags_begin(&flags); \
         __cli_mark == 0; \
        __cli_mark = _macro_cli_save_and_flags_end(&flags) \
    )

// Inputs: (n, )
#define NTH_BYTE(n, item) (((0xFF << (8*(n))) & (item)) >> (8*(n)))

#endif
#endif
