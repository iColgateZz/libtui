#include "terminal.h"
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/ioctl.h>

typedef struct Terminal {
    struct termios origTerm;
    struct termios newTerm;
} Terminal;

static Terminal t = {0};

static inline void enterAlternateBuffer(void);
static inline void leaveAlternateBuffer(void);
static inline void hideCursor(void);
static inline void showCursor(void);
static inline void enableCBreakMode(Terminal *t, b32 disableEcho);
static inline void disableCBreakMode(Terminal *t);
static inline void cleanupTerminal(void);
static inline void handleSignal(i32 sig);

void initTerminal(b32 disableEcho) {
    enterAlternateBuffer();
    enableCBreakMode(&t, disableEcho);
    hideCursor();

    atexit(cleanupTerminal);
    signal(SIGTERM, handleSignal);
    signal(SIGINT, handleSignal);
}


static inline void enterAlternateBuffer(void) {
    printf("\033[?1049h");
    fflush(stdout);
}

static inline void leaveAlternateBuffer(void) {
    printf("\033[?1049l");
    fflush(stdout);
}

static inline void hideCursor(void) {
    printf("\033[?25l");
    fflush(stdout);
}

static inline void showCursor(void) {
    printf("\033[?25h");
    fflush(stdout);
}

static inline void enableCBreakMode(Terminal *t, b32 disableEcho) {
    tcgetattr(STDIN_FILENO, &t->origTerm);

    t->newTerm = t->origTerm;
    t->newTerm.c_lflag &= ~(ICANON);
    if (disableEcho)
        t->newTerm.c_lflag &= ~(ECHO);

    tcsetattr(STDIN_FILENO, TCSANOW, &t->newTerm);
}

static inline void disableCBreakMode(Terminal *t) {
    tcsetattr(STDIN_FILENO, TCSANOW, &t->origTerm);
}

static inline void cleanupTerminal(void) {
    leaveAlternateBuffer();
    disableCBreakMode(&t);
    showCursor();
}

static inline void handleSignal(i32 sig) {
    cleanupTerminal();

    signal(sig, SIG_DFL);
    raise(sig);
}
