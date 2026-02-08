#define PSH_BUILD_IMPL
    #include "psh_build/psh_build.h"

#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <assert.h>

struct {
    struct termios orig_term;
} Terminal = {0};

void restore_term() {
    assert(tcsetattr(STDIN_FILENO, TCSAFLUSH, &Terminal.orig_term) == 0);
}

void init_term() {
    assert(tcgetattr(STDIN_FILENO, &Terminal.orig_term) == 0);
    atexit(restore_term);
    
    struct termios raw = Terminal.orig_term;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 10;

    assert(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0);
}

i32 main(i32 argc, byte *argv[]) {
    PSH_REBUILD_UNITY_AUTO(argc, argv);

    init_term();

    while (1) {
        char c = 0;
        read(STDIN_FILENO, &c, 1);

        printf("%d ('%c')\r\n", c, c);

        if (c == 'q') break;
    }

    return 0;
}