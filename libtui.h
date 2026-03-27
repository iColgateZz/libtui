#ifndef LIBTUI_INCLUDE
#define PSH_CORE_NO_PREFIX
#include "psh_build/psh_build.h"

// public

//TODO: make more memory efficient by encoding
//      raw_len and d_width in 1 byte
typedef struct {
    byte raw[4];
    u8 raw_len;
    u8 display_width;
} CodePoint;

#define cp(s)      cp_from_utf8(s, sizeof(s) - 1)
CodePoint cp_from_utf8(byte *s, u8 len);
CodePoint cp_from_raw(byte *raw, u8 raw_len, u8 display_width);
CodePoint cp_from_byte(byte b);
b32 cp_equal(CodePoint a, CodePoint b);

#define ctrl(x) cp_from_byte((x) & 0x1F)

#define CELL_REGULAR        0x00
#define CELL_CONTINUATION   0x01
#define CELL_WIDE_LEAD      0x02

//TODO: add color support
typedef struct {
    CodePoint cp;
    u8 flags;
} Cell;

Cell cell(CodePoint cp);
Cell cell_cont();
Cell cell_empty();
b32 cell_equal(Cell a, Cell b);

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

typedef enum {
    ENone = 0,
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
CodePoint get_codepoint();

typedef struct {
    i32 x, y, w, h;
} Rectangle;

b32 point_in_rect(i32 x, i32 y, Rectangle r);
Rectangle rect_intersect(Rectangle a, Rectangle b);
Rectangle rect_union(Rectangle a, Rectangle b);

typedef struct {
    Rectangle clip;
    i32 offset_x;
    i32 offset_y;
} Scope;

Scope pop_scope();
Scope peek_scope();
void push_scope(i32 x, i32 y, i32 w, i32 h);

void init_terminal();
void set_max_timeout_ms(i32 timeout);
void begin_frame();
void end_frame();
u64 get_delta_time();
u32 get_terminal_width();
u32 get_terminal_height();

typedef u32 Unicode;
Unicode utf8_decode(byte *s, usize len);
u8 unicode_width(Unicode ch);

void put_str(i32 x, i32 y, byte *str, usize len);
void put_codepoint(i32 x, i32 y, CodePoint cp);
void put_ascii_char(i32 x, i32 y, byte c);
void put_ascii_str(i32 x, i32 y, byte *str, usize len);

byte *vfmt(byte *p, byte *end, byte *f, va_list args);
byte *fmt(byte *p, byte *end, byte *f, ...);
byte *fmt_uint(byte *p, byte *end, u64 v, u8 base);
byte *fmt_cstr(byte *p, byte *end, byte *s);
byte *fmt_s8(byte *p, byte *end, s8 s);

typedef struct {
    byte *start;
    byte *p;
    byte *end;
} Stream;

Stream stream_start(byte *buffer, usize size);
void stream_fmt(Stream *s, byte *f, ...);
s8 stream_end(Stream s);
Stream arena_stream_start(Arena *arena, usize size);

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

da_typedef(CellBuffer, Cell);
da_typedef(Scopes, Scope);
da_typedef(ByteBuffer, byte);

struct {
    struct termios orig_term;
    Unix_Pipe pipe;
    i32 timeout;
    Event event;
    u64 saved_time, dt;
    CellBuffer frontbuffer;
    CellBuffer backbuffer;
    ByteBuffer frame_cmds;
    Arena tmp;
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
CodePoint get_codepoint() { return Terminal.event.parsed_cp; }

// private
void restore_term();
void update_screen_dimensions();
void handle_sigwinch(i32 signo);
i64  time_ms();
void save_timestamp();
void calculate_dt();
void poll_event(Event *e);
void parse_input(byte *str, isize n, Event *e);
void write_str_len(byte *str, usize len);
void write_strf_impl(byte *fmt, ...);
#define write_str(s)        write_str_len(s, sizeof(s) - 1)
#define write_strf(...)     write_strf_impl(__VA_ARGS__)
void generate_absolute_cursor_move(ByteBuffer *a, u32 row, u32 col);
void generate_relative_cursor_move(ByteBuffer *a, u32 step);
void render();
void update_root_scope();

b32 try_parse_mouse(byte *str, isize n, Event *e);
b32 try_parse_term_key(byte *str, isize n, Event *e);
b32 try_parse_text(byte *str, isize n, Event *e);
void fix_wide_char(i32 x, i32 y);
void emit_cells(ByteBuffer *out, Cell *cells, usize start, usize len);

typedef enum {
    UTF8_OK = 0,
    UTF8_INCOMPLETE,
    UTF8_FAIL,
} Utf8Status;

typedef struct {
    Utf8Status status;
    usize consumed_bytes;
    CodePoint cp;
} Utf8Result;

Utf8Result utf8_next(byte *s, usize len);

static CodePoint UTF8_REPLACEMENT = {
    .raw = {0xEF, 0xBF, 0xBD},
    .raw_len = 3,
    .display_width = 1,
};

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
    //TODO: maybe add something to deal with the cursor on the user side
    //      so that the user can manipulate the cursor position maybe
    //TODO: handle pasting into the terminal?

    write_str("\33[?2004l");                 // disable bracketed paste mode
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
    Rectangle r = {.w = Terminal.width, .h = Terminal.height};
    da_append(&Terminal.scopes, (Scope) {.clip = r});

    Terminal.tmp = arena_init(MB(16));
}

void update_screen_dimensions() {
    struct winsize ws = {0};
    assert(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0);

    Terminal.width = ws.ws_col;
    Terminal.height = ws.ws_row;
}

void update_root_scope() {
    Rectangle r = {.w = Terminal.width, .h = Terminal.height};
    Terminal.scopes.items[0] = (Scope) {.clip = r};
}

void restore_term() {
    assert(tcsetattr(STDIN_FILENO, TCSAFLUSH, &Terminal.orig_term) == 0);

    write_str("\33[?1000l");                     // disable mouse
    write_str("\33[?1002l");                     // disable mouse
    write_str("\33[0m");                         // reset text attributes
    write_str("\33[?25h");                       // show cursor
    write_str("\33[?1049l");                     // exit alternate buffer

    fd_close(Terminal.pipe.read_fd);
    fd_close(Terminal.pipe.write_fd);

    da_free(Terminal.backbuffer);
    da_free(Terminal.frontbuffer);
    da_free(Terminal.frame_cmds);
    da_free(Terminal.scopes);

    arena_destroy(Terminal.tmp);
}

void handle_sigwinch(i32 signo) {
    update_screen_dimensions();

    u32 new_size = Terminal.width * Terminal.height;
    da_resize(&Terminal.backbuffer, new_size);
    da_resize(&Terminal.frontbuffer, new_size);

    update_root_scope();

    // trigger full redraw
    for (usize i = 0; i < Terminal.frontbuffer.count; ++i)
        Terminal.frontbuffer.items[i] = cell(cp_from_byte(0xFF));

    write(Terminal.pipe.write_fd, &signo, sizeof signo);
}

void set_max_timeout_ms(i32 timeout) { Terminal.timeout = timeout; }

void begin_frame() {
    save_timestamp();

    arena_clear(&Terminal.tmp);
    da_clear(&Terminal.frame_cmds);
    for (usize i = 0; i < Terminal.backbuffer.count; ++i)
        Terminal.backbuffer.items[i] = cell_empty();

    Terminal.event = (Event) {0};
    poll_event(&Terminal.event);
}

void save_timestamp()  { Terminal.saved_time = time_ms(); }
void calculate_dt() { Terminal.dt = time_ms() - Terminal.saved_time; }

i64 time_ms() {
    struct timespec ts = {0};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void end_frame() {
    render();
    write_str_len(Terminal.frame_cmds.items, Terminal.frame_cmds.count);
    calculate_dt();
}

//TODO: maybe hash each row and compare hashes?
void render() {
    struct {
        u32 x, y;
    } cursor = {0};
    
    Cell *back_items = Terminal.backbuffer.items;
    Cell *front_items = Terminal.frontbuffer.items;
    u32 screen_w = Terminal.width;

    for (u32 row = 0; row < Terminal.height; row++) {
        usize row_start = row * screen_w;
        usize row_end   = row_start + screen_w;

        usize pos = row_start;
        b32 first_in_row = true;

        while (pos < row_end) {
            if (cell_equal(back_items[pos], front_items[pos])) {
                pos++;
                continue;
            }

            usize run_start = pos;
            while (pos < row_end && !cell_equal(back_items[pos], front_items[pos])) {
                pos++;
            }

            usize run_len = pos - run_start;
            u32 new_row = run_start / screen_w;
            u32 new_col = run_start % screen_w;

            if (first_in_row) {
                first_in_row = false;
                generate_absolute_cursor_move(&Terminal.frame_cmds, new_row, new_col);
            } else {
                generate_relative_cursor_move(&Terminal.frame_cmds, new_col - cursor.x);
            }

            emit_cells(&Terminal.frame_cmds, back_items, run_start, run_len);
            cursor.y = pos / screen_w;
            cursor.x = pos % screen_w;

            memcpy(
                front_items + run_start,
                back_items + run_start,
                run_len * sizeof(Cell)
            );
        }
    }
}

void emit_cells(ByteBuffer *out, Cell *cells, usize start, usize len) {
    for (usize i = 0; i < len; i++) {
        Cell c = cells[start + i];
        if (c.flags & CELL_CONTINUATION) continue;

        da_append_many(out, c.cp.raw, c.cp.raw_len);
    }
}

void generate_absolute_cursor_move(ByteBuffer *a, u32 row, u32 col) {
    Stream s = arena_stream_start(&Terminal.tmp, 64);
    stream_fmt(&s, "\33[%u;%uH", row + 1, col + 1);
    s8 result = stream_end(s);

    da_append_many(a, result.s, result.len);
}

void generate_relative_cursor_move(ByteBuffer *a, u32 step) {
    Stream s = arena_stream_start(&Terminal.tmp, 64);
    stream_fmt(&s, "\33[%uC", step);
    s8 result = stream_end(s);

    da_append_many(a, result.s, result.len);
}

void poll_event(Event *e) {
    #define PFD_SIZE 2
    struct pollfd pfd[PFD_SIZE] = {
        {.fd = Terminal.pipe.read_fd, .events = POLLIN},
        {.fd = STDIN_FILENO,          .events = POLLIN},
    };

    i32 timeout_ms = Terminal.timeout >= 0 ? Terminal.timeout : -1;
    i32 rval = poll(pfd, PFD_SIZE, timeout_ms);

    if (rval < 0) {
        if (errno == EAGAIN || errno == EINTR) {
            poll_event(e);
            return;
        }

        assert(false && "call to 'poll' failed");
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
        isize n = 0;
        static byte buffer[256];
        n = read(STDIN_FILENO, buffer, sizeof buffer);

        assert(n > 0 && "read non-positive amount of bytes from STDIN");

        parse_input(buffer, n, e);
        return;
    }
}

void parse_input(byte *str, isize n, Event *e) {
    if (str[0] == TERMKEY_ESCAPE) {
        if (try_parse_mouse(str, n, e)) return;
        if (try_parse_term_key(str, n, e)) return;

        // Unknown escape sequence
        e->type = ENone;
        return;
    }

    if (try_parse_text(str, n, e)) return;

    e->type = ENone;
}

b32 try_parse_mouse(byte *str, isize n, Event *e) {
    if (n < 9 || memcmp(str, "\33[<", 3) != 0) return false;

    byte *p = str;
    u32 btn          = strtol(p + 3, &p, 10);
    e->mouse.x       = strtol(p + 1, &p, 10) - 1;
    e->mouse.y       = strtol(p + 1, &p, 10) - 1;
    e->mouse.pressed = (*p == 'M');

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

b32 try_parse_term_key(byte *str, isize n, Event *e) {
    if (n < 3 || n > 4) return false;

    for (usize i = 0; i < ARRAY_SIZE(term_key_table); i++) {
        byte *key = term_key_table[i].str;
        if (memcmp(str + 1, key, n - 1) == 0) {
            e->type = ETermKey;
            e->term_key = term_key_table[i].k;
            return true;
        }
    }

    return false;
}

b32 try_parse_text(byte *str, isize n, Event *e) {
    if (1 <= n && n <= 4) {
        Utf8Result res = utf8_next(str, n);
        if (res.status == UTF8_INCOMPLETE) return false;
        
        e->type = ECodePoint;
        e->parsed_cp = res.cp;
        return true;
    }

    return false;
}

Utf8Result utf8_next(byte *s, usize len) {
    assert(len > 0 && "invalid length");

    u8 first = s[0];
    if (first < 0x80) {
        return (Utf8Result) {
            .status = UTF8_OK,
            .consumed_bytes = 1,
            .cp = cp_from_byte(first),
        };
    }

    usize expected_len = 0;
    if ((first & 0xE0) == 0xC0) expected_len = 2;
    else if ((first & 0xF0) == 0xE0) expected_len = 3;
    else if ((first & 0xF8) == 0xF0) expected_len = 4;
    else {
        return (Utf8Result) {
            .consumed_bytes = 1,
            .status = UTF8_FAIL,
            .cp = UTF8_REPLACEMENT,
        };
    }

    if (expected_len > len) {
        return (Utf8Result) { 
            .consumed_bytes = 0,
            .status = UTF8_INCOMPLETE,
            .cp = UTF8_REPLACEMENT,
        };
    }

    for (usize i = 1; i < expected_len; i++) {
        if ((s[i] & 0xC0) != 0x80) {
            return (Utf8Result) {
                .consumed_bytes = i,
                .status = UTF8_FAIL,
                .cp = UTF8_REPLACEMENT,
            };
        }
    }

    return (Utf8Result) {
        .consumed_bytes = expected_len,
        .status = UTF8_OK,
        .cp = cp_from_utf8(s, expected_len),
    };
}

Unicode utf8_decode(byte *s, usize len) {
    switch (len) {
        case 1: return s[0];
        case 2:
            return ((s[0] & 0x1F) << 6) |
                   (s[1] & 0x3F);
        case 3:
            return ((s[0] & 0x0F) << 12) |
                   ((s[1] & 0x3F) << 6) |
                   (s[2] & 0x3F);
        case 4:
            return ((s[0] & 0x07) << 18) |
                   ((s[1] & 0x3F) << 12) |
                   ((s[2] & 0x3F) << 6) |
                   (s[3] & 0x3F);
    }

    return 0xFFFD; // decoded replacement character
}

u8 unicode_width(Unicode ch) {
    if (ch < 32 || (ch >= 0x7F && ch < 0xA0))
        return 0;

    // combining marks
    if ((ch >= 0x0300 && ch <= 0x036F) ||
        (ch >= 0x1AB0 && ch <= 0x1AFF) ||
        (ch >= 0x1DC0 && ch <= 0x1DFF) ||
        (ch >= 0x20D0 && ch <= 0x20FF) ||
        (ch >= 0xFE20 && ch <= 0xFE2F))
        return 0;

    // wide characters (CJK + emoji)
    if ((ch >= 0x1100 && ch <= 0x115F) ||
        (ch >= 0x2329 && ch <= 0x232A) ||
        (ch >= 0x2E80 && ch <= 0xA4CF) ||
        (ch >= 0xAC00 && ch <= 0xD7A3) ||
        (ch >= 0xF900 && ch <= 0xFAFF) ||
        (ch >= 0xFE10 && ch <= 0xFE19) ||
        (ch >= 0xFE30 && ch <= 0xFE6F) ||
        (ch >= 0xFF00 && ch <= 0xFF60) ||
        (ch >= 0xFFE0 && ch <= 0xFFE6) ||
        (ch >= 0x1F300 && ch <= 0x1F64F) ||
        (ch >= 0x1F900 && ch <= 0x1F9FF))
        return 2;

    return 1;
}

CodePoint cp_from_utf8(byte *s, u8 len) {
    Unicode value = utf8_decode(s, len);
    return cp_from_raw(s, len, unicode_width(value));
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
        .raw_len = 1,
        .display_width = 1,
    };
}

b32 cp_equal(CodePoint a, CodePoint b) {
    if (a.raw_len != b.raw_len) return false;
    if (a.display_width != b.display_width) return false;
    return memcmp(a.raw, b.raw, a.raw_len) == 0;
}

Cell cell(CodePoint cp) { return (Cell) { .cp = cp }; }
Cell cell_lead(CodePoint cp) { return (Cell) { .cp = cp, .flags = CELL_WIDE_LEAD }; }
Cell cell_cont() { return (Cell) { .flags = CELL_CONTINUATION }; }
Cell cell_empty() { return (Cell) { .cp = cp_from_byte(' ') }; }
b32 cell_equal(Cell a, Cell b) { return a.flags == b.flags && cp_equal(a.cp, b.cp); }

void put_str(i32 x, i32 y, byte *s, usize len) {
    usize i = 0;
    while (i < len) {
        Utf8Result res = utf8_next(s + i, len - i);

        if (res.status == UTF8_INCOMPLETE) break;

        put_codepoint(x, y, res.cp);

        i += res.consumed_bytes;
        x += res.cp.display_width;
    }
}

void put_codepoint(i32 x, i32 y, CodePoint cp) {
    Scope parent = peek_scope();
    i32 tx = x + parent.offset_x;
    i32 ty = y + parent.offset_y;

    if (tx < 0 || ty < 0) return;
    if (!point_in_rect(tx, ty, parent.clip)) return;

    u32 w = Terminal.width;
    Cell *cells = Terminal.backbuffer.items;

    if (cp.display_width == 1) {
        fix_wide_char(tx, ty);
        cells[tx + ty * w] = cell(cp);
        return;
    }

    if (cp.display_width == 2) {
        if ((u32)tx + 1 >= w) return; // cannot fit

        fix_wide_char(tx, ty);
        fix_wide_char(tx + 1, ty);

        cells[tx + ty * w] = cell_lead(cp);
        cells[(tx + 1) + ty * w] = cell_cont();
    }

    // ignore other widths
    assert(false && "a codepoint with invalid width");
}

void fix_wide_char(i32 x, i32 y) {
    u32 w = Terminal.width;
    Cell *cells = Terminal.backbuffer.items;
    Cell c = cells[x + y * w];

    if ((c.flags & CELL_WIDE_LEAD) && (u32)x + 1 < w) {
        cells[(x + 1) + y * w] = cell_empty();
    }

    if ((c.flags & CELL_CONTINUATION) && (u32)x > 0) {
        cells[(x - 1) + y * w] = cell_empty();
    }
}

// void put_ascii_char(i32 x, i32 y, byte c) {
//     put_codepoint(x, y, cp_from_byte(c));
// }

// void put_ascii_str(i32 x, i32 y, byte *str, usize len) {
//     Rectangle parent = peek_scope();
//     if (!point_in_rect(x, y, parent)) return;

//     usize copy_len = MIN(len, parent.x + parent.w - x);
//     for (usize i = 0; i < copy_len; ++i) {
//         put_ascii_char(x + i, y, str[i]);
//     }
// }

void push_scope(i32 x, i32 y, i32 w, i32 h) {
    Scope parent = peek_scope();
    Rectangle clipped = rect_intersect(parent.clip, (Rectangle) {x, y, w, h});

    Scope s = {
        .clip = clipped, 
        .offset_x = parent.offset_x, 
        .offset_y = parent.offset_y
    };

    da_append(&Terminal.scopes, s);
}

Scope pop_scope() { 
    return da_pop(&Terminal.scopes);
}

Scope peek_scope() {
    return da_last(&Terminal.scopes);
}

void scope_translate(i32 dx, i32 dy) {
    Scope *s = &Terminal.scopes.items[Terminal.scopes.count - 1];
    s->offset_x += dx;
    s->offset_y += dy;
}

b32 point_in_rect(i32 x, i32 y, Rectangle r) {
    return r.x <= x && x < r.x + r.w 
        && r.y <= y && y < r.y + r.h;
}

Rectangle rect_intersect(Rectangle a, Rectangle b) {
    i32 x1 = MAX(a.x, b.x);
    i32 y1 = MAX(a.y, b.y);
    i32 x2 = MIN(a.x + a.w, b.x + b.w);
    i32 y2 = MIN(a.y + a.h, b.y + b.h);

    if (x2 <= x1 || y2 <= y1) {
        return (Rectangle){0,0,0,0}; // fully clipped
    }

    return (Rectangle){ x1, y1, x2 - x1, y2 - y1 };
}

Rectangle rect_union(Rectangle a, Rectangle b) {
    i32 left   = MIN(a.x, b.x);
    i32 top    = MIN(a.y, b.y);
    i32 right  = MAX(a.x + a.w, b.x + b.w);
    i32 bottom = MAX(a.y + a.h, b.y + b.h);

    return (Rectangle) {
        .x = left,
        .y = top,
        .w = right - left,
        .h = bottom - top
    };
}

byte *fmt(byte *p, byte *end, byte *f, ...) {
    va_list args;
    va_start(args, f);
    p = vfmt(p, end, f, args);
    va_end(args);
    return p;
}

// Behaviour is similar to snprintf, in that it does not
// write beyond the *end pointer, but the returned pointer
// may point somewhere beyond the *end. 
byte *vfmt(byte *p, byte *end, byte *f, va_list args) {
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

Stream stream_start(byte *buffer, usize size) {
    return (Stream) {
        .start = buffer,
        .p = buffer,
        .end = buffer + size
    };
}

void stream_fmt(Stream *s, byte *f, ...) {
    va_list args;
    va_start(args, f);
    s->p = vfmt(s->p, s->end, f, args);
    va_end(args);
}

s8 stream_end(Stream s) {
    usize len = MIN(s.p - s.start, s.end - s.start);
    return s8(s.start, len);
}

Stream arena_stream_start(Arena *arena, usize size) {
    byte *buffer = arena_push(arena, byte, size);
    assert(buffer && "arena does not have enough memory");
    return stream_start(buffer, size);
}

void debug(i32 x, i32 y, byte *fmt, ...) {
    Stream s = arena_stream_start(&Terminal.tmp, 256);

    va_list args;
    va_start(args, fmt);
    s.p = vfmt(s.p, s.end, fmt, args);
    va_end(args);

    s8 str = stream_end(s);
    put_str(x, y, str.s, str.len);
}

// TUI

void draw_line(i32 x0, i32 y0, i32 x1, i32 y1, CodePoint cp) {
    if (x0 == x1) { // vertical
        if (y1 < y0) {
            i32 tmp = y0;
            y0 = y1;
            y1 = tmp;
        }

        for (i32 y = y0; y <= y1; y++) {
            put_codepoint(x0, y, cp);
        }
    }
    else if (y0 == y1) { // horizontal
        if (x1 < x0) {
            i32 tmp = x0;
            x0 = x1;
            x1 = tmp;
        }

        for (i32 x = x0; x <= x1; x++) {
            put_codepoint(x, y0, cp);
        }
    }
}

void draw_box(Rectangle r) {
    if (r.w < 2 || r.h < 2) return;

    i32 x0 = r.x;
    i32 y0 = r.y;
    i32 x1 = r.x + r.w - 1;
    i32 y1 = r.y + r.h - 1;

    put_codepoint(x0, y0, cp("┌"));
    put_codepoint(x1, y0, cp("┐"));
    put_codepoint(x0, y1, cp("└"));
    put_codepoint(x1, y1, cp("┘"));

    draw_line(x0 + 1, y0, x1 - 1, y0, cp("─"));
    draw_line(x0 + 1, y1, x1 - 1, y1, cp("─"));
    draw_line(x0, y0 + 1, x0, y1 - 1, cp("│"));
    draw_line(x1, y0 + 1, x1, y1 - 1, cp("│"));
}

//TODO: add styling
typedef struct {
    u32 max_w;
    u32 max_h;
} LayoutConstraint;

typedef struct Widget Widget;

typedef struct {
    void (*measure)(Widget *self, LayoutConstraint constraint);
    void (*layout)(Widget *self);
    void (*update)(Widget *self);
    void (*draw)(Widget *self);
} WidgetVTable;

struct Widget {
    Rectangle rect;
    const WidgetVTable *vtable;

    u32 measured_w;
    u32 measured_h;
};

da_typedef(WidgetList, Widget *);

void widget_draw(Widget *w) { 
    // used for clipping
    // push_scope(w->rect.x, w->rect.y, w->rect.w, w->rect.h);
    w->vtable->draw(w); 
    // pop_scope();
}

void widget_update(Widget *w) { w->vtable->update(w); }
void widget_measure(Widget *w, LayoutConstraint c) { w->vtable->measure(w, c); }
void widget_layout(Widget *w) { w->vtable->layout(w); }

typedef struct {
    Widget widget;
    Widget *child;
    i64 y_offset;
} Screen;

void screen_measure(Widget *w, LayoutConstraint c) {
    w->measured_w = c.max_w;
    w->measured_h = c.max_h;

    Screen *s = container_of(w, Screen, widget);
    widget_measure(s->child, c);
}

void screen_layout(Widget *w) {
    w->rect.x = 0;
    w->rect.y = 0;
    w->rect.w = w->measured_w;
    w->rect.h = w->measured_h;

    Screen *s = container_of(w, Screen, widget);
    Widget *child = s->child;

    child->rect.x = (w->rect.w - child->measured_w) / 2;
    child->rect.y = (w->rect.h - child->measured_h) / 2;

    widget_layout(child);
}

void screen_update(Widget *w) {
    Screen *s = container_of(w, Screen, widget);

    if (is_event(EScrollDown)) {
        s->y_offset++;
    } else if (is_event(EScrollUp)) {
        s->y_offset = MAX(0, s->y_offset - 1);
    }

    widget_update(s->child);
}

void screen_draw(Widget *w) {
    Screen *s = container_of(w, Screen, widget);
    // debug(0, 0, "offset: %d", s->y_offset);
    push_scope(0, 0, w->rect.w, w->rect.h);
    scope_translate(0, -s->y_offset);
    widget_draw(s->child);
    pop_scope();
}

static const WidgetVTable screen_methods = {
    .measure = screen_measure,
    .layout = screen_layout,
    .update = screen_update,
    .draw = screen_draw,
};

Screen screen_new() {
    return (Screen) {.widget.vtable = &screen_methods};
}

static struct {
    Screen screen;
} UI = {0};

void ui_register_root(Widget *w) {
    UI.screen = screen_new();
    UI.screen.child = w;
}

void ui_run() {
    LayoutConstraint c = {
        .max_h = get_terminal_height(),
        .max_w = get_terminal_width(),
    };

    widget_measure(&UI.screen.widget, c);
    widget_layout(&UI.screen.widget);
    widget_update(&UI.screen.widget);
    widget_draw(&UI.screen.widget);
}

void draw_box_scrolled(Rectangle r) {
    u32 offset = MIN(r.y, UI.screen.y_offset);
    r.y -= offset;
    draw_box(r);
}

void put_str_scrolled(u32 x, u32 y, byte *s, usize len) {
    u32 offset = MIN(y, UI.screen.y_offset);
    u32 shifted_y = y - offset;
    put_str(x, shifted_y, s, len);
}

typedef struct {
    Widget widget;
    s8 label;
    b32 state;
} Button;

void button_draw(Widget *w) {
    Button *b = container_of(w, Button, widget);

    Rectangle r = w->rect;
    draw_box_scrolled(r);

    if (b->state) put_str_scrolled(r.x + 1, r.y + 1, b->label.s, b->label.len);
}

void button_update(Widget *w) {
    Button *b = container_of(w, Button, widget);

    Rectangle r = w->rect;
    if (is_event(EMouseLeft)) {
        u32 x = get_mouse_x();
        u32 y = get_mouse_y();

        if (point_in_rect(x, y, r) && is_mouse_pressed())
            b->state = !b->state;
    }
}

void button_measure(Widget *w, LayoutConstraint c) {
    Button *b = container_of(w, Button, widget);

    //TODO: account for text wrapping
    w->measured_h = 3;
    w->measured_w = b->label.len + 2;

    w->measured_w = MIN(w->measured_w, c.max_w);
}

void button_layout(Widget *w) {
    w->rect.w = w->measured_w;
    w->rect.h = w->measured_h;
}

static const WidgetVTable button_methods = {
    .draw = button_draw,
    .update = button_update,
    .measure = button_measure,
    .layout = button_layout,
};

Button button_new(s8 label) {
    Widget wid = { .vtable = &button_methods };
    return (Button) { .label = label, .widget = wid};
}

typedef struct {
    Widget widget;
    WidgetList children;
    u32 padding;
    u32 spacing;
} Div;

void div_measure(Widget *w, LayoutConstraint c) {
    Div *div = container_of(w, Div, widget);

    u32 inner_w = c.max_w - div->padding * 2;
    u32 inner_h = c.max_h - div->padding * 2;

    LayoutConstraint child_c = {
        .max_w = inner_w,
        .max_h = inner_h,
    };

    u32 width = 0;
    u32 height = div->padding * 2;

    for (usize i = 0; i < div->children.count; i++) {
        Widget *child = div->children.items[i];
        widget_measure(child, child_c);

        width = MAX(width, child->measured_w);
        height += child->measured_h;

        if (i < div->children.count - 1)
            height += div->spacing;
    }

    width += div->padding * 2;

    w->measured_w = MIN(width, c.max_w);
    w->measured_h = height;
}

void div_layout(Widget *w) {
    Div *div = container_of(w, Div, widget);

    w->rect.w = w->measured_w;
    w->rect.h = w->measured_h;

    u32 y = w->rect.y + div->padding;

    for (usize i = 0; i < div->children.count; i++) {
        Widget *child = div->children.items[i];
        
        child->rect.x = w->rect.x + (w->rect.w - child->measured_w) / 2;
        child->rect.y = y;

        y += child->rect.h + div->spacing;

        widget_layout(child);
    }
}

void div_update(Widget *w) {
    Div *div = container_of(w, Div, widget);
    for (usize i = 0; i < div->children.count; i++) {
        Widget *child = div->children.items[i];
        widget_update(child);
    }
}

void div_draw(Widget *w) {
    Div *div = container_of(w, Div, widget);
    draw_box_scrolled(w->rect);

    for (usize i = 0; i < div->children.count; i++) {
        Widget *child = div->children.items[i];
        widget_draw(child);
    }
}

static const WidgetVTable div_methods = {
    .measure = div_measure,
    .layout = div_layout,
    .update = div_update,
    .draw = div_draw,
};

Div div_new(u32 padding, u32 spacing) {
    Div b = {0};

    b.padding = padding;
    b.spacing = spacing;

    b.widget.vtable = &div_methods;

    return b;
}

void div_add(Div *div, Widget *child) { da_append(&div->children, child); }

#endif //LIBTUI_IMPL
