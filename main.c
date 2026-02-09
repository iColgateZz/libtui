#define PSH_BUILD_IMPL
    #include "psh_build/psh_build.h"

#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>

struct {
    struct termios orig_term;
} Terminal = {0};

void write_str_len(byte *str, usize len) {
    write(STDOUT_FILENO, str, len);
}

#define write_str(s)    write_str_len(s, sizeof(s) - 1)

void restore_term() {
    assert(tcsetattr(STDIN_FILENO, TCSAFLUSH, &Terminal.orig_term) == 0);

    write_str("\33[?1000l");                     // disable mouse
    write_str("\33[?1002l");                     // disable mouse
    write_str("\33[m");                          // reset colors
    write_str("\33[?25h");                       // show cursor
    write_str("\33[?1049l");                     // exit alternate buffer

    //TODO: If I print an error and the return to the original buffer,
    // the error will not be visible to the user.
}

void init_term() {
    assert(tcgetattr(STDIN_FILENO, &Terminal.orig_term) == 0);
    
    struct termios raw = Terminal.orig_term;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 10;

    write_str("\33[?2004l");                 // reset bracketed paste mode
    write_str("\33[?1049h");                 // use alternate buffer
    write_str("\33[?25l");                   // hide cursor
    write_str("\33[?1000h");                 // enable mouse
    write_str("\33[?1002h");                 // enable button events

    assert(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0);

    atexit(restore_term);
}

i32 main(i32 argc, byte *argv[]) {
    PSH_REBUILD_UNITY_AUTO(argc, argv);

    init_term();

    while (1) {
        char c = 0;
        read(STDIN_FILENO, &c, 1);

        if (iscntrl(c)) {
            printf("%d\r\n", c);
        } else {
            printf("%d ('%c')\r\n", c, c);
        }

        if (c == 'q') break;
    }

    return 0;
}