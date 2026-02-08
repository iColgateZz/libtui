#ifndef WINDOW_HEADER
#define WINDOW_HEADER

#include "types.h"
#include "widget.h"
#include "props.h"

typedef struct Window Window;

Window *initWindow(i32 x, i32 y, i32 width, i32 height);
void addWidget(Window *window, Widget *widget);
void updateWindow(Window *window, byte *screenBuffer, Props props);
void destroyWindow(Window *window);

#endif