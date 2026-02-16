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
    EWinch,
} EventType;

typedef struct {
    EventType type;
    byte buf[32];
    Key parsed_key;
} Event;

void init_terminal();
void set_max_timeout_ms(u32 timeout);
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
void put_str(u32 x, u32 y, byte *str, usize len);

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

typedef struct {
    Arena arena;
    byte *items;
    usize count;
    usize capacity;
} Array;

Array array_init(usize reserve_size) {
    Arena arena = arena_init(GB(1));

    byte *arr = arena_push(&arena, byte, reserve_size);
    assert(arr);
    memset(arr, 0, reserve_size);

    return (Array) {
        .arena = arena,
        .items = arr,
        .count = reserve_size,
        .capacity = arena.committed_size
    };
}

void array_extend_to(Array *a, usize new_size) {
    if (new_size > a->capacity) {
        arena_clear(&a->arena);
        a->items = arena_push(&a->arena, byte, new_size);
        assert(a->items);
        a->capacity = a->arena.committed_size;
    }
}

void array_append(Array *a, byte *item, usize len) {
    if (a->count + len >= a->capacity) {
        array_extend_to(a, a->count + len);
    }

    memcpy(a->items + a->count, item, len);
    a->count += len;
}

void array_destroy(Array a) { arena_destroy(a.arena); }

struct {
    struct termios orig_term;
    Unix_Pipe pipe;
    u32 timeout;
    Event event;
    u64 saved_time, dt;
    Array frontbuffer;
    Array backbuffer;
    Array frame_cmds;
    u32 width, height;
} Terminal = {0};

void _generate_cursor_move(Array *a, u32 old_row, u32 old_col, u32 new_row, u32 new_col);
usize _u32_to_ascii(byte *dst, u32 value);

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
    static char buf[1024];
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

    Terminal.backbuffer  = array_init(Terminal.width * Terminal.height);
    Terminal.frontbuffer = array_init(Terminal.width * Terminal.height);
    Terminal.frame_cmds  = array_init(0);
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

    array_destroy(Terminal.backbuffer);
    array_destroy(Terminal.frontbuffer);
    array_destroy(Terminal.frame_cmds);
}

void _handle_sigwinch(i32 signo) {
    _update_screen_dimensions();

    u32 new_size = Terminal.width * Terminal.height;
    array_extend_to(&Terminal.backbuffer, new_size);
    array_extend_to(&Terminal.frontbuffer, new_size);
    Terminal.backbuffer.count  = new_size;
    Terminal.frontbuffer.count = new_size;

    // trigger full redraw
    memset(Terminal.frontbuffer.items, 0, Terminal.frontbuffer.count);

    write(Terminal.pipe.write_fd, &signo, sizeof signo);
}

void set_max_timeout_ms(u32 timeout) { Terminal.timeout = timeout; }

void begin_frame() {
    memset(Terminal.backbuffer.items, ' ', Terminal.backbuffer.count);

    _save_timestamp();
    _poll_input();
}

void _save_timestamp()  { Terminal.saved_time = _time_ms(); }

i64 _time_ms() {
    struct timespec ts = {0};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

#define GAP_THRESHOLD 8
void end_frame() {
    _calculate_dt();

    Terminal.frame_cmds.count = 0;
    array_append(&Terminal.frame_cmds, "\33[H", 3);

    u32 w = Terminal.width;
    u32 h = Terminal.height;
    usize total = w * h;

    struct {
        u32 x, y;
    } cursor = {0};

    usize pos = 0;
    usize gap = 0;
    while (pos < total) {
        if (Terminal.backbuffer.items[pos] ==
            Terminal.frontbuffer.items[pos]) {
            pos++;
            gap++;
            continue;
        }

        usize run_start = pos;
        while (pos < total &&
               Terminal.backbuffer.items[pos] !=
               Terminal.frontbuffer.items[pos]) {
            pos++;
        }

        usize run_len = pos - run_start;
        if (gap <= GAP_THRESHOLD) {
            // instead of emitting cursor move, just copy the bytes
            // that are the same in both buffers
            array_append(&Terminal.frame_cmds, Terminal.backbuffer.items + run_start - gap, run_len + gap);
        } else {
            u32 new_row = run_start / w;
            u32 new_col = run_start % w;
            _generate_cursor_move(&Terminal.frame_cmds, cursor.y, cursor.x, new_row, new_col);
            array_append(&Terminal.frame_cmds, Terminal.backbuffer.items + run_start, run_len);
        }

        cursor.y = pos / w;
        cursor.x = pos % w;
        gap = 0;
        // Maybe use relative cursor move if it is cheaper. 
        // E.g. instead of go to (x, y), go 1 unit down.

        // Maybe instead of storing raw bytes, emit commands
        // (new struct) and store them. Later some logic may
        // handle the commands, reorder them or something

        // Everything above is probably more than enough but
        // if I feel fancy I can also implement dirty rectangle
        // optimization: track where the user writes and mark
        // these areas as dirty. Instead of diffing the whole
        // buffer, diff only the dirty rectangles.

        memcpy(
            Terminal.frontbuffer.items + run_start,
            Terminal.backbuffer.items + run_start,
            run_len
        );
    }

    _write_str_len(Terminal.frame_cmds.items, Terminal.frame_cmds.count);
}

void _calculate_dt() { Terminal.dt = _time_ms() - Terminal.saved_time; }

// absolute: 4 + len(x) + len(y)
// relative 3 + len(d)
// relative move is indeed cheaper, however
// there is a bug when using it and I have
// no idea what causes it
void _generate_cursor_move(Array *a, u32 old_row, u32 old_col, u32 new_row, u32 new_col) {
    byte tmp[64];
    byte *p = tmp;
    UNUSED(old_col);
    UNUSED(old_row);

    // if (old_row == new_row) {
    //     *p++ = '\33';
    //     *p++ = '[';
    //     p += _u32_to_ascii(p, new_col - old_col);
    //     *p++ = 'C';
    // } else {
        *p++ = '\33';
        *p++ = '[';
        p += _u32_to_ascii(p, new_row + 1);
        *p++ = ';';
        p += _u32_to_ascii(p, new_col + 1);
        *p++ = 'H';
    // }

    array_append(a, tmp, (usize)(p - tmp));
}

usize _u32_to_ascii(byte *dst, u32 value) {
    byte tmp[16];
    usize len = 0;

    do {
        tmp[len++] = '0' + (value % 10);
        value /= 10;
    } while (value);

    for (usize i = 0; i < len; i++)
        dst[i] = tmp[len - 1 - i];

    return len;
}

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
        e->type = EWinch;
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
    if (x >= Terminal.width || y >= Terminal.height) return;
    Terminal.backbuffer.items[x + y * Terminal.width] = c;
}

void put_str(u32 x, u32 y, byte *str, usize len) {
    if (x >= Terminal.width || y >= Terminal.height) return;

    // usize copy_len = MIN(len, Terminal.width - x); // clip at row end
    usize copy_len = MIN(len, Terminal.width * Terminal.height - (x + y * Terminal.width));
    memcpy(Terminal.backbuffer.items + x + y * Terminal.width, str, copy_len);
}

#endif //LIBTUI_IMPL
