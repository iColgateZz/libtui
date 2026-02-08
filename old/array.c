#include <string.h>
#include <stdlib.h>
#include "err.h"
#include "array.h"

#define DEFAULT_CAPACITY 4

Array arrayInit(isize elemSize) {
    Array arr = {
        .buffer = malloc(DEFAULT_CAPACITY * elemSize),
        .cap = DEFAULT_CAPACITY,
        .size = 0,
        .elemSize = elemSize
    };

    if (!arr.buffer)
        throw("array: allocation failure");

    return arr;
}

static inline void arrayResize(Array *a) {
    a->cap *= 2;
    void *buffer = realloc(a->buffer, a->cap * a->elemSize);
    if (!buffer) 
        throw("array: reallocation failure");

    a->buffer = buffer;
}

void arrayPush(Array *a, void *elem) {
    if (a->size >= a->cap)
        arrayResize(a);
    
    memcpy((byte *)a->buffer + a->size * a->elemSize, elem, a->elemSize);
    ++a->size;
}

void *arrayGet(Array *a, isize idx) {
    if (idx < a->size) {
        return (char *)a->buffer + idx * a->elemSize;
    }

    throw("array: index out of bounds");
}

void arrayFree(Array *a) {
    free(a->buffer);
}