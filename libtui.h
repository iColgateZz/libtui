#ifndef LIBTUI_INCLUDE
#define PSH_CORE_NO_PREFIX
#include "psh_build/psh_build.h"

void init_term();
void restore_term();
void write_str_len(byte *str, usize len);
void write_strf_impl(byte *fmt, ...);
void get_screen_dimensions();
void handle_sigwinch(i32 signo);
void set_target_fps(u32 fps);

#define write_str(s)        write_str_len(s, sizeof(s) - 1)
#define write_strf(...)     write_strf_impl(__VA_ARGS__)

#define MAX(a, b)     ((a) > (b) ? (a) : (b))
#define MIN(a, b)     ((a) < (b) ? (a) : (b))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#endif //LIBTUI_INCLUDE

#ifdef LIBTUI_IMPL

#include <unistd.h>
#include <termios.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <errno.h>

typedef enum {
    BACKSPACE_KEY = 8,
    TAB_KEY       = 9,
    ENTER_KEY     = 13,
    ESCAPE_KEY    = 27,
    INSERT_KEY    = -1,
    DELETE_KEY    = -2,
    HOME_KEY      = -3,
    END_KEY       = -4,
    PAGEUP_KEY    = -5,
    PAGEDOWN_KEY  = -6,
    UP_KEY        = -7,
    DOWN_KEY      = -8,
    LEFT_KEY      = -9,
    RIGHT_KEY     = -10,
} Key;

static struct {byte str[4]; i32 k;} key_table[] = {
    {"[A" , UP_KEY},
    {"[B" , DOWN_KEY},
    {"[C" , RIGHT_KEY},
    {"[D" , LEFT_KEY},
    {"[2~", INSERT_KEY},
    {"[3~", DELETE_KEY},
    {"[H" , HOME_KEY},
    {"[4~", END_KEY},
    {"[5~", PAGEUP_KEY},
    {"[6~", PAGEDOWN_KEY},
};

typedef enum {
    EDraw,
    EKey,
} PollEventType;

typedef struct {
    PollEventType type;
    byte buf[32];
    i32 parsed_key;
} PollEvent;


typedef struct Widget Widget;

struct {
    struct termios orig_term;
    u16 width, height;
    Unix_Pipe pipe;
    u32 timeout;
    PollEvent event;
    i64 saved_time, dt;
    struct {
        usize count;
        usize capacity;
        Widget **items;
    } widgets;
    usize cringe;
} Terminal = {0};

void parse_event(PollEvent *e, isize n);
b32 escaped(byte *);
Key get_key();

void write_str_len(byte *str, usize len) {
    write(STDOUT_FILENO, str, len);
}

void write_strf_impl(byte *fmt, ...) {
    char buf[1024];
    va_list args;

    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0) {
        if (len > (int)sizeof(buf))
            len = sizeof(buf);
        write(STDOUT_FILENO, buf, len);
    }
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

    fd_close(Terminal.pipe.read_fd);
    fd_close(Terminal.pipe.write_fd);

    da_free(Terminal.widgets);
}

void handle_sigwinch(i32 signo) {
    write(Terminal.pipe.write_fd, &signo, sizeof signo);
}

void set_target_fps(u32 fps) {
    if (fps == 0){
        Terminal.timeout = 0;
    } else {
        Terminal.timeout = 1000 / fps;
    }
}

void poll_input() {
    #define PFD_SIZE 2
    struct pollfd pfd[PFD_SIZE] = {
        {.fd = Terminal.pipe.read_fd, .events = POLLIN},
        {.fd = STDIN_FILENO,          .events = POLLIN},
    };

    PollEvent *e = &Terminal.event;
    *e = (PollEvent) {0};

    i32 timeout_ms = Terminal.timeout > 0 ? Terminal.timeout : -1;
    i32 rval = poll(pfd, PFD_SIZE, timeout_ms);

    if (rval < 0) {
        if (errno == EAGAIN || errno == EINTR) {
            poll_input();
            return;
        }

        assert(false);
    }

    else if (rval == 0) { // timeout
        e->type = EDraw;
        return;
    }

    if (pfd[0].revents & POLLIN) { // window resize
        i32 sig;
        read(Terminal.pipe.read_fd, &sig, sizeof sig);

        e->type = EDraw;
        get_screen_dimensions();
        return;
    }

    if (pfd[1].revents & POLLIN) {
        isize n = read(STDIN_FILENO, e->buf, sizeof(e->buf) - 1);
        assert(n > 0);
        parse_event(e, n);
    }
}

void parse_event(PollEvent *e, isize n) {
    byte *str = e->buf;

    if (n == 1 && !escaped(str)) { // regular key
        e->type = EKey;

        #define DELETE 127
        if (str[0] == DELETE) {
            e->parsed_key = BACKSPACE_KEY;
        } else {
            // maybe decode ut8 later
            e->parsed_key = str[0];
        }

        return;
    }

    if ((n == 3 || n == 4) && escaped(str)) { // longer escaped sequence
        for (usize i = 0; i < ARRAY_SIZE(key_table); i++) {
            if (memcmp(str + 1, key_table[i].str, n - 1) == EXIT_SUCCESS) {
                e->type = EKey;
                e->parsed_key  = key_table[i].k;
                return;
            }
        }
    }

    e->type = EDraw;
    return;
}

b32 escaped(byte *s) { return s[0] == ESCAPE_KEY; }

Key get_key() { return Terminal.event.parsed_key; }

b32 key_pressed(Key k) {
    return Terminal.event.parsed_key == k 
        && Terminal.event.type == EKey;
}

i64 time_ms() {
    struct timespec ts = {0};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void save_timestamp()  { Terminal.saved_time = time_ms(); }
void calculate_dt() { Terminal.dt = time_ms() - Terminal.saved_time; }

struct Widget {
    u32 x, y;
    u32 w, h;
    void (*on_click)(Widget *);
    void (*on_hover)(Widget *);
    void (*draw)(Widget *);
};

void add_widget(Widget *w) {
    da_append(&Terminal.widgets, w);
}

void draw_widgets() {
    write_str("\33[H");
    for (usize i = 0; i < Terminal.widgets.count; ++i) {
        Widget *w = Terminal.widgets.items[i];
        w->draw(w);
    }
}

#endif //LIBTUI_IMPL
