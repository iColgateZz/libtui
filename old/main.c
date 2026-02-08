#include "terminal.h"
#include <time.h>
#include <stdio.h>
#include "screen.h"
#include "window.h"
#include <unistd.h>

#define TARGET_FPS 10000
#define BILLION 1000000000LL
#define FRAME_TIME_NS (BILLION / TARGET_FPS)

static inline i64 now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * BILLION + ts.tv_nsec;
}

int main() {
    // initTerminal(1);
    // Screen *screen = initScreen();

    // Window *window = initWindow(0, 0, 100, 20);
    // Widget *widget = initWidget(0, 0, 10, 10);

    // addWidget(window, widget);
    // addWindow(screen, window);

    // while (1) {
    //     i64 frame_start = now_ns();

    //     refreshScreen(screen);

    //     i64 frame_end = now_ns();
    //     i64 sleep_ns = FRAME_TIME_NS - (frame_end - frame_start);
    //     if (sleep_ns > 0) {
    //         struct timespec ts;
    //         ts.tv_sec = sleep_ns / BILLION;
    //         ts.tv_nsec = sleep_ns % BILLION;
    //         nanosleep(&ts, NULL);
    //     }
    // }

    printf("⠓, %lu\n", sizeof("⠓"));

    return 0;
}