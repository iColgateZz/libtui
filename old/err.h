#ifndef OUT_INCLUDE
#define OUT_INCLUDE

#include "types.h"
#include <stdio.h>

#define throw(format, ...) \
        do { \
            fprintf(stderr, format "\n", ##__VA_ARGS__); \
            exit(EXIT_FAILURE); \
        } while (0);

#ifdef DEBUG
    #define dbg_print(format, ...) \
        fprintf(stderr, "[%s : %d : %s()] " format "\n", \
                __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#else
    #define dbg_print(...) do {} while (0)
#endif

#endif