#include "window.h"
#include "window_def.h"
#include "err.h"
#include "array.h"
#include "widget.h"
#include "props.h"

Window *initWindow(i32 x, i32 y, i32 width, i32 height) {
    Window *win = malloc(sizeof(*win));

    if (!win) throw("window: allocation failure");

    *win = (Window) {
        .x = x,
        .y = y,
        .width = width,
        .height = height,
        .widgets = arrayInit(sizeof(Widget *))
    };

    return win;
}

void addWidget(Window *w, Widget *wid) {
    arrayPush(&w->widgets, &wid);
}

void updateWindow(Window *w, byte *screenBuffer, Props sProps) {
    arrayForEach(&w->widgets, Widget *, widget) {
        updateWidget(widget, screenBuffer, sProps.width, sProps.height);
    }

    for (i32 i = w->y; i < w->height + w->y && i < sProps.height; ++i) {
        for (i32 j = w->x; j < w->width + w->x && j < sProps.width; ++j) {

            if (i == w->y || i == w->y + w->height - 1)
                screenBuffer[i * sProps.width + j] = '$';
            
            else if (j == w->x || j == w->x + w->width - 1)
                screenBuffer[i * sProps.width + j] = '$';
        }
    }
}

void destroyWindow(Window *w) {
    arrayForEach(&w->widgets, Widget *, widget) {
        destroyWidget(widget);
    }

    arrayFree(&w->widgets);
    free(w);
}

