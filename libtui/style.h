#ifndef LIBTUI_STYLE_INCLUDE
#define LIBTUI_STYLE_INCLUDE

#define PSH_CORE_NO_PREFIX
#include "psh_core/psh_core.h"

typedef enum {
    FIT,
    FILL,
    FIXED,
} SizeMode;

typedef struct {
    SizeMode mode;
    union {
        i32 value; //FIXED
        struct { //FIT, FILL
            i32 min;
            i32 max;
        };
    };
} AxisSize;

typedef struct {
    AxisSize w;
    AxisSize h;
} Sizing;

typedef struct {
    u8 r, g, b;
} Color;

#endif