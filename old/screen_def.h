#ifndef SCREEN_DEF_HEADER
#define SCREEN_DEF_HEADER

#include "types.h"
#include "window_def.h"
#include "array.h"
#include "props.h"

struct Screen {
    byte *buffer;
    Props props;
    Array windows;
    // input handler
    // mouse click handler
};

#endif