#ifndef WINDOW_DEF_HEADER
#define WINDOW_DEF_HEADER

#include "types.h"
#include "array.h"

struct Window {
    i32 x, y;
    i32 width, height;
    Array widgets;
};

#endif