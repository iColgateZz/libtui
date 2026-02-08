#ifndef MACROS_INCLUDE
#define MACROS_INCLUDE

#include <stddef.h>

#define foreach(type, buf, n)       for (type iter = buf; iter < (type)buf + n; ++iter)

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#endif