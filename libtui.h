#ifndef LIBTUI_INCLUDE
#define PSH_CORE_NO_PREFIX
#include "psh_build/psh_build.h"

// public
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

typedef enum {
    EDraw,
    EKey,
} EventType;

typedef struct {
    EventType type;
    byte buf[32];
    Key parsed_key;
} Event;

typedef struct Widget Widget;
struct Widget {
    u32 x, y;
    u32 w, h;
    void (*draw)(Widget *);
};

void init_terminal();
void set_target_fps(u32 fps);
void begin_frame();
void end_frame();
u64 get_delta_time();
u32 get_terminal_width();
u32 get_terminal_height();
EventType get_event_type();
Event get_event();
Key get_key();
b32 is_key_pressed(Key k);

void put_char(u32 x, u32 y, byte c);

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
#include <string.h>

#define MAX(a, b)     ((a) > (b) ? (a) : (b))
#define MIN(a, b)     ((a) < (b) ? (a) : (b))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

// private
void _restore_term();
void _update_screen_dimensions();
void _handle_sigwinch(i32 signo);
i64  _time_ms();
void _save_timestamp();
void _calculate_dt();
void _parse_event(Event *e, isize n);
void _poll_input();
void _write_str_len(byte *str, usize len);
void _write_strf_impl(byte *fmt, ...);
#define write_str(s)        _write_str_len(s, sizeof(s) - 1)
#define write_strf(...)     _write_strf_impl(__VA_ARGS__)

struct {
    struct termios orig_term;
    Unix_Pipe pipe;
    u32 timeout;
    Event event;
    u64 saved_time, dt;
    Arena arena;
    byte *framebuffer;
    u32 width, height;
} Terminal = {0};

u64 get_delta_time() { return Terminal.dt; }
u32 get_terminal_width() { return Terminal.width; }
u32 get_terminal_height() { return Terminal.height; }
EventType get_event_type() { return Terminal.event.type; }
Event get_event() { return Terminal.event; }
Key get_key() { return Terminal.event.parsed_key; }
b32 is_key_pressed(Key k) { return Terminal.event.parsed_key == k 
                            && Terminal.event.type == EKey; }

void _write_str_len(byte *str, usize len) {
    write(STDOUT_FILENO, str, len);
}

void _write_strf_impl(byte *fmt, ...) {
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

void init_terminal() {
    assert(tcgetattr(STDIN_FILENO, &Terminal.orig_term) == 0);
    
    struct termios raw = Terminal.orig_term;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

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
    write_str("\33[H");                     // move cursor to home position

    _update_screen_dimensions();
    atexit(_restore_term);
    assert(pipe_open(&Terminal.pipe));

    struct sigaction sa = {0};
    sa.sa_handler = _handle_sigwinch;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sa, NULL);

    Terminal.arena = arena_init(GB(1));
    Terminal.framebuffer = arena_push(&Terminal.arena, byte, Terminal.width * Terminal.height);
    assert(Terminal.framebuffer);
}

void _update_screen_dimensions() {
    struct winsize ws = {0};
    assert(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0);

    Terminal.width = ws.ws_col;
    Terminal.height = ws.ws_row;
}

void _restore_term() {
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

    arena_destroy(Terminal.arena);
}

void _handle_sigwinch(i32 signo) {
    _update_screen_dimensions();
    if (Terminal.width * Terminal.height > Terminal.arena.committed_size) {
        arena_clear(&Terminal.arena);
        arena_push(&Terminal.arena, byte, Terminal.width * Terminal.height);
    }

    write(Terminal.pipe.write_fd, &signo, sizeof signo);
}

void set_target_fps(u32 fps) {
    if (fps == 0){
        Terminal.timeout = 0;
    } else {
        Terminal.timeout = 1000 / fps;
    }
}

void begin_frame() {
    memset(Terminal.framebuffer, 0, 
        Terminal.width * Terminal.height * sizeof(*Terminal.framebuffer));

    _save_timestamp();
    _poll_input();
}

void _save_timestamp()  { Terminal.saved_time = _time_ms(); }

i64 _time_ms() {
    struct timespec ts = {0};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void end_frame() {
    write_str("\33[H"); // move cursor to home position
    _calculate_dt();

    for (u32 y = 0; y < Terminal.height; ++y) {
        _write_str_len(Terminal.framebuffer + y * Terminal.width, Terminal.width);
        // if (y < Terminal.height - 1)
        // write_str("\r\n");
    }
}

void _calculate_dt() { Terminal.dt = _time_ms() - Terminal.saved_time; }

void _poll_input() {
    #define PFD_SIZE 2
    struct pollfd pfd[PFD_SIZE] = {
        {.fd = Terminal.pipe.read_fd, .events = POLLIN},
        {.fd = STDIN_FILENO,          .events = POLLIN},
    };

    Event *e = &Terminal.event;
    *e = (Event) {0};

    i32 timeout_ms = Terminal.timeout > 0 ? Terminal.timeout : -1;
    i32 rval = poll(pfd, PFD_SIZE, timeout_ms);

    if (rval < 0) {
        if (errno == EAGAIN || errno == EINTR) {
            _poll_input();
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
        return;
    }

    if (pfd[1].revents & POLLIN) {
        isize n = read(STDIN_FILENO, e->buf, sizeof(e->buf) - 1);
        assert(n > 0);
        _parse_event(e, n);
    }
}

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

void _parse_event(Event *e, isize n) {
    byte *str = e->buf;

    if (n == 1 && str[0] != ESCAPE_KEY) { // regular key
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

    if ((n == 3 || n == 4) && str[0] == ESCAPE_KEY) { // longer escaped sequence
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

void put_char(u32 x, u32 y, byte c) {
    Terminal.framebuffer[x + y * Terminal.width] = c;
}

#endif //LIBTUI_IMPL
