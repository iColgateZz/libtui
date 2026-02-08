#ifndef STRING_INCLUDE
#define STRING_INCLUDE

#include "types.h"

typedef struct {
    byte *s;
    isize len;
} s8;

typedef s8 s8_; // function pointer return type

#define countof(x)                      (isize)(sizeof(x) / sizeof(*(x)))
#define lenof(s)                        countof(s) - 1

#define s8(...)                         s8x(__VA_ARGS__, s8_from_heap, s8_read_only)(__VA_ARGS__)
#define s8x(a, b, c, ...)               c
#define s8_read_only(str)               (s8){.s = str, .len = lenof(str)}
#define s8_from_heap(str, str_len)      ß(s8){.s = str, .len = str_len}

#endif