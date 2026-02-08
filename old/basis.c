#include "basis.h"
#include <stdio.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <unistd.h>

void clear(void) {
    printf("\033[2J");
    fflush(stdout);
}

void flush(void) {
    fflush(stdout);
}

void getTermDimensions(i32 *x, i32 *y) {
    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    *y = ws.ws_row - 1;
    *x = ws.ws_col;
}

void mvCursor(i32 x, i32 y) {
    printf("\033[%d;%dH", y, x);
}

void mvCursorUp(i32 num_lines) {
    printf("\033[%dA", num_lines);
}

void mvCursorDown(i32 num_lines) {
    printf("\033[%dB", num_lines);
}

void mvCursorRight(i32 num_cols) {
    printf("\033[%dC", num_cols);
}

void mvCursorStartOfLine(void) {
    printf("\r");
}

void mvCursorLeft(i32 num_cols) {
    printf("\033[%dD", num_cols);
}



void drawCharXY(byte ch, i32 x, i32 y) {
    mvCursor(x, y);
    drawChar(ch);
}

void drawChar(byte ch) {
    fputc(ch, stdout);
}

void drawS8XY(s8 s, i32 x, i32 y) {
    mvCursor(x, y);
    drawS8(s);
}

void drawS8(s8 s) {
    for (isize i = 0; i < s.len; ++i)
        fputc(s.s[i], stdout);
}
