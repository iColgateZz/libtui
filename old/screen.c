#include "screen.h"
#include "basis.h"
#include "window.h"
#include "screen_def.h"
#include "err.h"
#include <string.h>

static Screen screen = {0};

Screen* initScreen(void) {
    getTermDimensions(&screen.props.width, &screen.props.height);

    screen.buffer = malloc(screen.props.width * screen.props.height + 1);
    if (!screen.buffer) {
        throw("screen: allocation failure");
    }
    memset(screen.buffer, ' ', screen.props.width * screen.props.height);
    screen.buffer[screen.props.width * screen.props.height] = 0;

    screen.windows = arrayInit(sizeof(Window *));

    return &screen;
}

void addWindow(Screen *screen, Window *window) {
    arrayPush(&screen->windows, &window);
}

void refreshScreen(Screen *s) {
    arrayForEach(&s->windows, Window *, win) {
        updateWindow(win, s->buffer, s->props);
    }

    mvCursor(0, 0);
    printf("%s", s->buffer);
    flush();
}

void destroyScreen(Screen *s) {
    arrayForEach(&s->windows, Window *, win) {
        destroyWindow(win);
    }

    free(screen.buffer);
    arrayFree(&screen.windows);
}
