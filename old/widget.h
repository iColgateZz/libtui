#ifndef WIDGET_HEADER
#define WIDGET_HEADER

#include "types.h"

typedef struct Widget {
    i32 x, y;
    i32 width, height;
} Widget;

Widget *initWidget(i32 x, i32 y, i32 width, i32 height);
void updateWidget(Widget *widget, byte *screenBuffer, i32 maxX, i32 maxY);
void destroyWidget(Widget *widget);

#endif