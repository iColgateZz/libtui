#ifndef LIBTUI_INCLUDE
#define PSH_CORE_NO_PREFIX
#include "psh_build/psh_build.h"

void init_term();
void restore_term();
void write_str_len(byte *str, usize len);
void get_screen_dimensions();
void handle_sigwinch(i32 signo);

#define write_str(s)    write_str_len(s, sizeof(s) - 1)

#endif //LIBTUI_INCLUDE

#ifdef LIBTUI_IMPL

#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <signal.h>

struct {
    struct termios orig_term;
    u16 width, height;
    Unix_Pipe pipe;
} Terminal = {0};

void write_str_len(byte *str, usize len) {
    write(STDOUT_FILENO, str, len);
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

    assert(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0);

    write_str("\33[?2004l");                 // reset bracketed paste mode
    write_str("\33[?1049h");                 // use alternate buffer
    write_str("\33[?25l");                   // hide cursor
    // write_str("\33[?1000h");                 // enable mouse press/release
    // write_str("\33[?1002h");                 // enable mouse press/release + drag
    // write_str("\33[?1003h");                 // enable mouse press/release + drag + hover
    // write_str("\33[?1006h");                 // use mouse sgr protocol
    write_str("\33[0m");                     // reset text attributes
    write_str("\33[2J");                     // clear screen

    get_screen_dimensions();
    atexit(restore_term);
    assert(pipe_open(&Terminal.pipe));

    struct sigaction sa = {0};
    sa.sa_handler = handle_sigwinch;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sa, NULL);
}

void get_screen_dimensions() {
    struct winsize ws = {0};
    assert(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0);

    Terminal.width = ws.ws_col;
    Terminal.height = ws.ws_row;
}

void restore_term() {
    assert(tcsetattr(STDIN_FILENO, TCSAFLUSH, &Terminal.orig_term) == 0);

    // write_str("\33[?1000l");                     // disable mouse
    // write_str("\33[?1002l");                     // disable mouse
    write_str("\33[0m");                         // reset text attributes
    write_str("\33[?25h");                       // show cursor
    write_str("\33[?1049l");                     // exit alternate buffer

    //TODO: If I print an error and the return to the original buffer,
    // the error will not be visible to the user.
}

void handle_sigwinch(i32 signo) {
    write(Terminal.pipe.write_fd, &signo, sizeof signo);
}

#endif //LIBTUI_IMPL
