#include "widget.h"
#include "err.h"
#include "basis.h"

Widget *initWidget(i32 x, i32 y, i32 width, i32 height) {
    Widget *widget = malloc(sizeof(*widget));

    if (!widget) throw("widget: allocation failure");

    *widget = (Widget) {
        .x = x,
        .y = y,
        .width = width,
        .height = height
    };

    return widget;
}

void updateWidget(Widget *w, byte *screenBuffer, i32 maxX, i32 maxY) {
    for (i32 i = w->y; i < w->height + w->y && i < maxY; ++i) {
        for (i32 j = w->x; j < w->width + w->x && j < maxX; ++j) {

            if (i == w->y || i == w->y + w->height - 1)
                screenBuffer[i * maxX + j] = '#';
            
            else if (j == w->x || j == w->x + w->width - 1)
                screenBuffer[i * maxX + j] = '#';

            else 
                screenBuffer[i * maxX + j] = 'A';
        }
    }
}

void destroyWidget(Widget *w) {
    free(w);
}