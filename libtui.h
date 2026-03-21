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
    u32 x, y, w, h;
} Rectangle;

b32 point_in_rect(u32 x, u32 y, Rectangle r);
Rectangle rect_intersect(Rectangle a, Rectangle b);
Rectangle rect_union(Rectangle a, Rectangle b);

Rectangle pop_scope();
Rectangle peek_scope();
void push_scope(u32 x, u32 y, u32 w, u32 h);

void init_terminal();
void set_max_timeout_ms(u32 timeout);
void begin_frame();
void end_frame();
u64 get_delta_time();
u32 get_terminal_width();
u32 get_terminal_height();

typedef u32 Unicode;
Unicode utf8_decode(byte *s, usize len);
u8 unicode_width(Unicode ch);

void put_str(u32 x, u32 y, byte *str, usize len);
void put_codepoint(u32 x, u32 y, CodePoint cp);
void put_ascii_char(u32 x, u32 y, byte c);
void put_ascii_str(u32 x, u32 y, byte *str, usize len);

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
da_typedef(Scopes, Rectangle);
da_typedef(ByteBuffer, byte);

//TODO: add some inner state that will be 
//      rendered to the screen for debugging
struct {
    struct termios orig_term;
    Unix_Pipe pipe;
    u32 timeout;
    Event event;
    u64 saved_time, dt;
    CellBuffer frontbuffer;
    CellBuffer backbuffer;
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
void fix_wide_char(u32 x, u32 y);
void emit_cells(ByteBuffer *out, Cell *cells, usize start, usize len);

typedef enum {
    PARSE_OK = 0,
    PARSE_NEED_MORE,
    PARSE_FAIL
} ParseStatus;

typedef struct {
    CodePoint cp;
    ParseStatus status;
    usize consumed_bytes;
} UTF8ParseResult;

UTF8ParseResult utf8_try_from(byte *s, usize len);

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
    //      the error will not be visible to the user.
    //      Global state error like errno?

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
        Terminal.frontbuffer.items[i] = cell(cp_from_byte(0xFF));

    write(Terminal.pipe.write_fd, &signo, sizeof signo);
}

void set_max_timeout_ms(u32 timeout) { Terminal.timeout = timeout; }

void begin_frame() {
    save_timestamp();

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

//TODO: maybe add an arena for temporary allocations?
void generate_absolute_cursor_move(ByteBuffer *a, u32 row, u32 col) {
    byte tmp[64];
    Stream s = stream_start(tmp, 64);

    stream_fmt(&s, "\33[%u;%uH", row + 1, col + 1);
    s8 result = stream_end(s);

    da_append_many(a, result.s, result.len);
}

void generate_relative_cursor_move(ByteBuffer *a, u32 step) {
    byte tmp[64];
    Stream s = stream_start(tmp, 64);

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

    i32 timeout_ms = Terminal.timeout > 0 ? Terminal.timeout : -1;
    i32 rval = poll(pfd, PFD_SIZE, timeout_ms);

    if (rval < 0) {
        if (errno == EAGAIN || errno == EINTR) {
            poll_event(e);
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
        isize n = 0;
        static byte buffer[256];
        n = read(STDIN_FILENO, buffer, sizeof buffer);

        assert(n > 0);

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
    u32 btn         = strtol(p + 3, &p, 10);
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
        // TODO: what if given 4 bytes, but only 1
        //       is decoded due to an error?
        e->type = ECodePoint;
        UTF8ParseResult res = utf8_try_from(str, n);
        e->parsed_cp = res.cp;
        return true;
    }

    return false;
}

UTF8ParseResult utf8_try_from(byte *s, usize len) {
    assert(len > 0);

    u8 first = s[0];
    if (first < 0x80) {
        return (UTF8ParseResult) {
            .cp = cp_from_byte(first),
            .consumed_bytes = 1,
        };
    }

    usize expected_len = 0;
    if ((first & 0xE0) == 0xC0) expected_len = 2;
    else if ((first & 0xF0) == 0xE0) expected_len = 3;
    else if ((first & 0xF8) == 0xF0) expected_len = 4;
    else {
        return (UTF8ParseResult) {
            .consumed_bytes = 1,
            .status = PARSE_FAIL,
        };
    }

    if (expected_len > len) {
        return (UTF8ParseResult) { 
            .consumed_bytes = 0,
            .status = PARSE_NEED_MORE,
        };
    }

    for (usize i = 1; i < expected_len; i++) {
        if ((s[i] & 0xC0) != 0x80) {
            return (UTF8ParseResult) {
                .consumed_bytes = i,
                .status = PARSE_FAIL,
            };
        }
    }

    Unicode decoded_char = utf8_decode(s, expected_len);
    u8 width = unicode_width(decoded_char);

    return (UTF8ParseResult) {
        .cp = cp_from_raw(s, expected_len, width),
        .consumed_bytes = expected_len,
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

void put_str(u32 x, u32 y, byte *s, usize len) {
    usize i = 0;
    while (i < len) {
        UTF8ParseResult result = utf8_try_from(s + i, len - i);

        CodePoint cp = result.cp;
        if (result.status == PARSE_OK) {
            cp = result.cp;
        } else if (result.status == PARSE_FAIL) {
            cp = UTF8_REPLACEMENT;
        } else { // PARSE_NEED_MORE
            // should not happen
            cp = UTF8_REPLACEMENT;
            assert(false);
        }

        put_codepoint(x, y, cp);

        i += result.consumed_bytes;
        x += cp.display_width;
    }
}

void put_codepoint(u32 x, u32 y, CodePoint cp) {
    Rectangle parent = peek_scope();
    if (!point_in_rect(x, y, parent)) return;

    u32 w = Terminal.width;
    Cell *cells = Terminal.backbuffer.items;

    if (cp.display_width == 1) {
        fix_wide_char(x, y);
        cells[x + y * w] = cell(cp);
        return;
    }

    if (cp.display_width == 2) {
        if (x + 1 >= w) return; // cannot fit

        fix_wide_char(x, y);
        fix_wide_char(x + 1, y);

        cells[x + y * w] = cell_lead(cp);
        cells[(x + 1) + y * w] = cell_cont();
    }

    // ignore other widths
}

void fix_wide_char(u32 x, u32 y) {
    u32 w = Terminal.width;
    Cell *cells = Terminal.backbuffer.items;
    Cell c = cells[x + y * w];

    if ((c.flags & CELL_WIDE_LEAD) && x + 1 < w) {
        cells[(x + 1) + y * w] = cell_empty();
    }

    if ((c.flags & CELL_CONTINUATION) && x > 0) {
        cells[(x - 1) + y * w] = cell_empty();
    }
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
    assert(buffer);
    return stream_start(buffer, size);
}

#endif //LIBTUI_IMPL
