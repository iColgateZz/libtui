#ifndef LIBTUI_INCLUDE
#define PSH_CORE_NO_PREFIX
#include "psh_build/psh_build.h"

// public

typedef enum {
    TERMKEY_BACKSPACE = 8,
    TERMKEY_TAB       = 9,
    TERMKEY_ENTER     = 13,
    TERMKEY_ESCAPE    = 27,

    TERMKEY_INSERT    = -1,
    TERMKEY_DELETE    = -2,
    TERMKEY_HOME      = -3,
    TERMKEY_END       = -4,
    TERMKEY_PAGEUP    = -5,
    TERMKEY_PAGEDOWN  = -6,
    TERMKEY_UP        = -7,
    TERMKEY_DOWN      = -8,
    TERMKEY_LEFT      = -9,
    TERMKEY_RIGHT     = -10,
} TermKey;

typedef struct {
    byte raw[4];
    u8 raw_len;
    u8 display_width;
} CodePoint;

#define CP_ASSUMED_WIDTH 1
#define cp(cp)      cp_from_raw(cp, sizeof(cp) - 1, CP_ASSUMED_WIDTH)
CodePoint cp_from_raw(byte *raw, u8 raw_len, u8 display_width);
CodePoint cp_from_byte(byte b);
b32 cp_equal(CodePoint a, CodePoint b);

typedef enum {
    ENone,
    ETermKey,
    ECodePoint,
    EWinch,
    EMouseLeft,
    EMouseRight,
    EMouseMiddle,
    EScrollUp,
    EScrollDown,
    EMouseDrag,
} EventType;

typedef struct {
    EventType type;
    byte buf[32];
    union {
        struct {
            u32 x, y;
            b32 pressed;
        } mouse;
        CodePoint parsed_cp;
        TermKey term_key;
    };
} Event;

b32 is_event(EventType e);
u32 get_mouse_x();
u32 get_mouse_y();
b32 is_mouse_pressed();
b32 is_mouse_released();
b32 is_term_key(TermKey k);
b32 is_codepoint(CodePoint cp);

typedef struct {
    u32 x, y, w, h;
} Rectangle;

b32 point_in_rect(u32 x, u32 y, Rectangle r);
Rectangle rect_intersect(Rectangle a, Rectangle b);
Rectangle rect_union(Rectangle a, Rectangle b);

da_typedef(CP_Buffer, CodePoint);
da_typedef(Scopes, Rectangle);
da_typedef(ByteBuffer, byte);

void init_terminal();
void set_max_timeout_ms(u32 timeout);
void begin_frame();
void end_frame();
u64 get_delta_time();
u32 get_terminal_width();
u32 get_terminal_height();

void put_str(u32 x, u32 y, byte *str, usize len);
void put_codepoint(u32 x, u32 y, CodePoint cp);
void put_ascii_char(u32 x, u32 y, byte c);
void put_ascii_str(u32 x, u32 y, byte *str, usize len);

byte *fmt(byte *p, byte *end, byte *f, ...);
byte *fmt_uint(byte *p, byte *end, u64 v, u8 base);
byte *fmt_cstr(byte *p, byte *end, byte *s);
byte *fmt_s8(byte *p, byte *end, s8 s);

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

struct {
    struct termios orig_term;
    Unix_Pipe pipe;
    u32 timeout;
    Event event;
    u64 saved_time, dt;
    CP_Buffer frontbuffer;
    CP_Buffer backbuffer;
    ByteBuffer frame_cmds;
    Scopes scopes;
    u32 width, height;
} Terminal = {0};

u64 get_delta_time() { return Terminal.dt; }
u32 get_terminal_width() { return Terminal.width; }
u32 get_terminal_height() { return Terminal.height; }
b32 is_event(EventType e) { return Terminal.event.type == e; }
u32 get_mouse_x() { return Terminal.event.mouse.x; }
u32 get_mouse_y() { return Terminal.event.mouse.y; }
b32 is_mouse_pressed() { return Terminal.event.mouse.pressed; }
b32 is_mouse_released() { return !is_mouse_pressed(); }
b32 is_term_key(TermKey k) { return Terminal.event.term_key == k && Terminal.event.type == ETermKey; }
b32 is_codepoint(CodePoint cp) { return cp_equal(cp, Terminal.event.parsed_cp); }

// private
void restore_term();
void update_screen_dimensions();
void handle_sigwinch(i32 signo);
i64  time_ms();
void save_timestamp();
void calculate_dt();
void parse_event(Event *e, isize n);
void poll_input();
void write_str_len(byte *str, usize len);
void write_strf_impl(byte *fmt, ...);
#define write_str(s)        write_str_len(s, sizeof(s) - 1)
#define write_strf(...)     write_strf_impl(__VA_ARGS__)
void generate_absolute_cursor_move(ByteBuffer *a, u32 row, u32 col);
void generate_relative_cursor_move(ByteBuffer *a, u32 step);
Rectangle pop_scope();
Rectangle peek_scope();
void push_scope(u32 x, u32 y, u32 w, u32 h);
void render();
void update_root_scope();
b32 try_parse_mouse(Event *e, byte *str, isize n);
b32 try_parse_term_key(Event *e, byte *str, isize n);
b32 try_parse_text(Event *e, byte *str, isize n);
CodePoint try_decode_utf8(byte *s, usize len, usize *consumed);

void write_str_len(byte *str, usize len) {
    write(STDOUT_FILENO, str, len);
}

void write_strf_impl(byte *fmt, ...) {
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
    write_str("\33[?1000h");                 // enable mouse press/release
    write_str("\33[?1002h");                 // enable mouse press/release + drag
    // write_str("\33[?1003h");                 // enable mouse press/release + drag + hover
    write_str("\33[?1006h");                 // use mouse sgr protocol
    write_str("\33[0m");                     // reset text attributes
    write_str("\33[2J");                     // clear screen
    write_str("\33[H");                     // move cursor to home position

    update_screen_dimensions();
    atexit(restore_term);
    assert(pipe_open(&Terminal.pipe));

    struct sigaction sa = {0};
    sa.sa_handler = handle_sigwinch;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sa, NULL);

    da_resize(&Terminal.backbuffer, Terminal.width * Terminal.height);
    da_resize(&Terminal.frontbuffer, Terminal.width * Terminal.height);
    da_resize(&Terminal.frame_cmds, Terminal.width * Terminal.height);

    // manually add the terminal scope
    da_append(&Terminal.scopes, ((Rectangle) {.w = Terminal.width, .h = Terminal.height}));
}

void update_screen_dimensions() {
    struct winsize ws = {0};
    assert(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0);

    Terminal.width = ws.ws_col;
    Terminal.height = ws.ws_row;
}

void update_root_scope() {
    Terminal.scopes.items[0] = (Rectangle) {
        .w = Terminal.width,
        .h = Terminal.height,
    };
}

void restore_term() {
    assert(tcsetattr(STDIN_FILENO, TCSAFLUSH, &Terminal.orig_term) == 0);

    write_str("\33[?1000l");                     // disable mouse
    write_str("\33[?1002l");                     // disable mouse
    write_str("\33[0m");                         // reset text attributes
    write_str("\33[?25h");                       // show cursor
    write_str("\33[?1049l");                     // exit alternate buffer

    //TODO: If I print an error and the return to the original buffer,
    // the error will not be visible to the user.

    fd_close(Terminal.pipe.read_fd);
    fd_close(Terminal.pipe.write_fd);

    da_free(Terminal.backbuffer);
    da_free(Terminal.frontbuffer);
    da_free(Terminal.frame_cmds);
    da_free(Terminal.scopes);
}

void handle_sigwinch(i32 signo) {
    update_screen_dimensions();

    u32 new_size = Terminal.width * Terminal.height;
    da_resize(&Terminal.backbuffer, new_size);
    da_resize(&Terminal.frontbuffer, new_size);

    update_root_scope();

    // trigger full redraw
    for (usize i = 0; i < Terminal.frontbuffer.count; ++i)
        Terminal.frontbuffer.items[i] = cp_from_byte(0xFF);

    write(Terminal.pipe.write_fd, &signo, sizeof signo);
}

void set_max_timeout_ms(u32 timeout) { Terminal.timeout = timeout; }

void begin_frame() {
    save_timestamp();
    for (usize i = 0; i < Terminal.backbuffer.count; ++i)
        Terminal.backbuffer.items[i] = cp_from_byte(' ');

    poll_input();
}

void save_timestamp()  { Terminal.saved_time = time_ms(); }
void calculate_dt() { Terminal.dt = time_ms() - Terminal.saved_time; }

i64 time_ms() {
    struct timespec ts = {0};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

#define GAP_THRESHOLD 8
void end_frame() {
    Terminal.frame_cmds.count = 0;
    render();
    write_str_len(Terminal.frame_cmds.items, Terminal.frame_cmds.count);
    calculate_dt();
}

// The code relies on the assumption that every cell has a codepoint
// with a display_width = 1. If it is not the case, it breaks.
void render() {
    struct {
        u32 x, y;
    } cursor = {0};
    
    u32 screen_w = Terminal.width;
    for (u32 row = 0; row < Terminal.height; row++) {

        usize row_start = row * screen_w;
        usize row_end   = row_start + screen_w;

        usize pos = row_start;
        usize gap = 0;
        b32 first_in_row = true;

        CodePoint *back_items = Terminal.backbuffer.items;
        CodePoint *front_items = Terminal.frontbuffer.items;

        while (pos < row_end) {

            if (cp_equal(back_items[pos], front_items[pos])) {
                pos++;
                gap++;
                continue;
            }

            usize run_start = pos;

            while (pos < row_end && !cp_equal(back_items[pos], front_items[pos])) {
                pos++;
            }

            usize run_len = pos - run_start;

            if (first_in_row) {
                first_in_row = false;
                u32 new_row = run_start / screen_w;
                u32 new_col = run_start % screen_w;
                generate_absolute_cursor_move(&Terminal.frame_cmds, new_row, new_col);
                for (usize i = 0; i < run_len; ++i) {
                    da_append_many(&Terminal.frame_cmds, back_items[run_start + i].raw, back_items[run_start + i].raw_len);
                }
            } else if (gap <= GAP_THRESHOLD) {
                // instead of emitting cursor move, just copy the bytes
                // that are the same in both buffers
                for (usize i = 0; i < run_len + gap; ++i) {
                    da_append_many(&Terminal.frame_cmds, back_items[run_start - gap + i].raw, back_items[run_start - gap + i].raw_len);
                }
            } else {
                // I know it is the same row
                u32 new_col = run_start % screen_w;
                generate_relative_cursor_move(&Terminal.frame_cmds, new_col - cursor.x);
                for (usize i = 0; i < run_len; ++i) {
                    da_append_many(&Terminal.frame_cmds, back_items[run_start + i].raw, back_items[run_start + i].raw_len);
                }
            }

            gap = 0;
            cursor.y = pos / screen_w;
            cursor.x = pos % screen_w;

            memcpy(
                front_items + run_start,
                back_items + run_start,
                run_len * sizeof(CodePoint)
            );
        }
    }
}

void generate_absolute_cursor_move(ByteBuffer *a, u32 row, u32 col) {
    byte tmp[64];
    byte *p = tmp;
    byte *end = tmp + sizeof tmp;

    p = fmt(p, end, "\33[%u;%uH", row + 1, col + 1);
    usize len = MIN((usize)(p - tmp), sizeof tmp);
    da_append_many(a, tmp, len);
}

void generate_relative_cursor_move(ByteBuffer *a, u32 step) {
    byte tmp[64];
    byte *p = tmp;
    byte *end = tmp + sizeof tmp;

    p = fmt(p, end, "\33[%uC", step);
    usize len = MIN((usize)(p - tmp), sizeof tmp);
    da_append_many(a, tmp, len);
}

void poll_input() {
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
            poll_input();
            return;
        }

        assert(false);
    }

    else if (rval == 0) { // timeout
        e->type = ENone;
        return;
    }

    if (pfd[0].revents & POLLIN) { // window resize
        i32 sig;
        read(Terminal.pipe.read_fd, &sig, sizeof sig);
        e->type = EWinch;
        return;
    }

    if (pfd[1].revents & POLLIN) {
        isize n = read(STDIN_FILENO, e->buf, sizeof(e->buf));
        assert(n > 0);
        parse_event(e, n);
    }
}

void parse_event(Event *e, isize n) {
    byte *str = e->buf;

    if (str[0] == TERMKEY_ESCAPE) {
        if (try_parse_mouse(e, str, n))    return;
        if (try_parse_term_key(e, str, n)) return;

        // Unknown escape sequence
        e->type = ENone;
        return;
    }

    // special case
    #define DELETE 127
    if (n == 1 && str[0] == DELETE) {
        e->type = ETermKey;
        e->term_key = TERMKEY_BACKSPACE;
        return;
    }

    if (try_parse_text(e, str, n)) return;

    e->type = ENone;
}

b32 try_parse_mouse(Event *e, byte *str, isize n) {
    if (n < 9 || memcmp(str, "\33[<", 3) != 0)
        return false;

    u32 btn          = strtol(str + 3, &str, 10);
    e->mouse.x       = strtol(str + 1, &str, 10) - 1;
    e->mouse.y       = strtol(str + 1, &str, 10) - 1;
    e->mouse.pressed = (*str == 'M');

    switch (btn) {
        case 0:  e->type = EMouseLeft;   break;
        case 1:  e->type = EMouseMiddle; break;
        case 2:  e->type = EMouseRight;  break;
        case 32: e->type = EMouseDrag;   break;
        case 64: e->type = EScrollUp;    break;
        case 65: e->type = EScrollDown;  break;
        default: e->type = ENone;
    }

    return true;
}

static struct {byte str[4]; TermKey k;} term_key_table[] = {
    {"[A" , TERMKEY_UP},
    {"[B" , TERMKEY_DOWN},
    {"[C" , TERMKEY_RIGHT},
    {"[D" , TERMKEY_LEFT},
    {"[2~", TERMKEY_INSERT},
    {"[3~", TERMKEY_DELETE},
    {"[H" , TERMKEY_HOME},
    {"[4~", TERMKEY_END},
    {"[5~", TERMKEY_PAGEUP},
    {"[6~", TERMKEY_PAGEDOWN},
};

b32 try_parse_term_key(Event *e, byte *str, isize n) {
    if (!(3 <= n && n <= 4))
        return false;

    for (usize i = 0; i < ARRAY_SIZE(term_key_table); i++) {
        if (memcmp(str + 1, term_key_table[i].str, n - 1) == 0) {
            e->type = ETermKey;
            e->term_key = term_key_table[i].k;
            return true;
        }
    }

    return false;
}

b32 try_parse_text(Event *e, byte *str, isize n) {
    if (1 <= n && n <= 4) {
        // TODO: proper utf-8 handling
        // By default assumes the display_width of a cp is 1.
        // Never checks if input is correct utf-8.
        e->type = ECodePoint;
        e->parsed_cp = cp_from_raw(str, n, CP_ASSUMED_WIDTH);
        return true;
    }

    return false;
}

CodePoint cp_from_raw(byte *raw, u8 raw_len, u8 display_width) {
    CodePoint cp = {
        .raw_len = raw_len,
        .display_width = display_width,
    };

    memcpy(cp.raw, raw, raw_len);
    return cp;
}

CodePoint cp_from_byte(byte b) {
    return (CodePoint) {
        .raw = {b},
        .display_width = 1,
        .raw_len = 1,
    };
}

b32 cp_equal(CodePoint a, CodePoint b) {
    if (a.raw_len != b.raw_len) return false;
    if (a.display_width != b.display_width) return false;
    return memcmp(a.raw, b.raw, a.raw_len) == 0;
}

static CodePoint UTF8_REPLACEMENT = {
    .raw = {0xEF, 0xBF, 0xBD},
    .raw_len = 3,
    .display_width = 1,
};

CodePoint try_decode_utf8(byte *s, usize len, usize *consumed) {
    assert(len > 0);

    u8 first = s[0];
    if (first < 0x80) {
        *consumed = 1;
        return cp_from_byte(first);
    }

    usize expected_len = 0;
    if ((first & 0xE0) == 0xC0) expected_len = 2;
    else if ((first & 0xF0) == 0xE0) expected_len = 3;
    else if ((first & 0xF8) == 0xF0) expected_len = 4;
    else {
        *consumed = 1;
        return UTF8_REPLACEMENT;
    }

    assert(expected_len <= len);
    for (usize i = 1; i < expected_len; i++) {
        if ((s[i] & 0xC0) != 0x80) {
            *consumed = i;
            return UTF8_REPLACEMENT;
        }
    }

    *consumed = expected_len;
    return cp_from_raw(s, expected_len, CP_ASSUMED_WIDTH);
}

void put_str(u32 x, u32 y, byte *s, usize len) {
    usize i = 0;
    while (i < len) {
        usize consumed = 0;
        CodePoint cp = try_decode_utf8(s + i, len - i, &consumed);
        put_codepoint(x, y, cp);
        i += consumed;
        x += cp.display_width;
    }
}

void put_codepoint(u32 x, u32 y, CodePoint cp) {
    Rectangle parent = peek_scope();
    if (!point_in_rect(x, y, parent)) return;

    CodePoint *back_items = Terminal.backbuffer.items;
    back_items[x + y * Terminal.width] = cp;
}

//TODO: these 2 functions are not necessarily needed
//      put_str may be used instead of put_ascii_str
void put_ascii_char(u32 x, u32 y, byte c) {
    put_codepoint(x, y, cp_from_byte(c));
}

void put_ascii_str(u32 x, u32 y, byte *str, usize len) {
    Rectangle parent = peek_scope();
    if (!point_in_rect(x, y, parent)) return;

    usize copy_len = MIN(len, parent.x + parent.w - x);
    for (usize i = 0; i < copy_len; ++i) {
        put_ascii_char(x + i, y, str[i]);
    }
}

void push_scope(u32 x, u32 y, u32 w, u32 h) {
    Rectangle parent = peek_scope();
    Rectangle clipped = rect_intersect(parent, (Rectangle) {x, y, w, h});
    da_append(&Terminal.scopes, clipped);
}

Rectangle pop_scope() { 
    return da_pop(&Terminal.scopes);
}

Rectangle peek_scope() {
    return da_last(&Terminal.scopes);
}

b32 point_in_rect(u32 x, u32 y, Rectangle r) {
    return r.x <= x && x < r.x + r.w 
        && r.y <= y && y < r.y + r.h;
}

Rectangle rect_intersect(Rectangle a, Rectangle b) {
    u32 x1 = MAX(a.x, b.x);
    u32 y1 = MAX(a.y, b.y);
    u32 x2 = MIN(a.x + a.w, b.x + b.w);
    u32 y2 = MIN(a.y + a.h, b.y + b.h);

    if (x2 <= x1 || y2 <= y1) {
        return (Rectangle){0,0,0,0}; // fully clipped
    }

    return (Rectangle){ x1, y1, x2 - x1, y2 - y1 };
}

Rectangle rect_union(Rectangle a, Rectangle b) {
    u32 left   = MIN(a.x, b.x);
    u32 top    = MIN(a.y, b.y);
    u32 right  = MAX(a.x + a.w, b.x + b.w);
    u32 bottom = MAX(a.y + a.h, b.y + b.h);

    return (Rectangle) {
        .x = left,
        .y = top,
        .w = right - left,
        .h = bottom - top
    };
}

// Behaviour is similar to snprintf, in that it does not
// write beyond the *end pointer, but the returned pointer
// may point somewhere beyond the *end. 
byte *fmt(byte *p, byte *end, byte *f, ...) {
    va_list args;
    va_start(args, f);

    while (*f) {
        if (*f != '%') {
            if (p < end) *p = *f;
            p++;
            f++;
            continue;
        }

        f++;
        switch (*f++) {
            case 'd': {
                i64 v = va_arg(args, i64);

                if (v < 0) {
                    if (p < end) *p = '-';
                    p++;

                    u64 u = (u64)(-v);
                    p = fmt_uint(p, end, u, 10);
                } else {
                    p = fmt_uint(p, end, (u64)v, 10);
                }

            } break;

            case 'u': {
                u64 v = va_arg(args, u64);
                p = fmt_uint(p, end, v, 10);
            } break;

            case 'x': {
                u64 v = va_arg(args, u64);
                p = fmt_uint(p, end, v, 16);
            } break;

            case 's': {
                byte *s = va_arg(args, byte *);
                p = fmt_cstr(p, end, s);
            } break;

            case 'S': {
                s8 s = va_arg(args, s8);
                p = fmt_s8(p, end, s);
            } break;

            case '%': {
                if (p < end) *p = '%';
                p++;
            } break;
        }
    }

    va_end(args);
    return p;
}

byte *fmt_uint(byte *p, byte *end, u64 v, u8 base) {
    byte tmp[32];
    usize n = 0;

    do {
        u8 d = v % base;
        tmp[n++] = (d < 10) ? '0' + d : 'a' + d - 10;
        v /= base;
    } while (v);

    while (n--) {
        if (p < end) *p = tmp[n];
        p++;
    }

    return p;
}

byte *fmt_cstr(byte *p, byte *end, byte *s) {
    while (*s) {
        if (p < end) *p = *s;
        p++;
        s++;
    }
    return p;
}

byte *fmt_s8(byte *p, byte *end, s8 s) {
    for (usize i = 0; i < s.len; i++) {
        if (p < end) *p = s.s[i];
        p++;
    }
    return p;
}


#endif //LIBTUI_IMPL
