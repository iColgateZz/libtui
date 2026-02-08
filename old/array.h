#ifndef ARRAY_HEADER
#define ARRAY_HEADER

#include "types.h"

typedef struct Array {
    void *buffer;
    isize cap;
    isize size;
    isize elemSize;
} Array;

Array arrayInit(isize elemSize);
void arrayPush(Array *a, void *elem);
void *arrayGet(Array *a, isize idx);
void arrayFree(Array *a);

#define arrayForEach(arr_p, type, iter) \
    for (isize _i = 0; _i < (arr_p)->size; ++_i) \
        for (type iter = *(type *)((char *)(arr_p)->buffer + (_i) * (arr_p)->elemSize); _i < (arr_p)->size; _i = (arr_p)->size)


#endif