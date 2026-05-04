#ifndef LIBTUI_RENDERER_INCLUDE
#define LIBTUI_RENDERER_INCLUDE

#define PSH_CORE_NO_PREFIX
#include "psh_core/psh_core.h"

//TODO: make more memory efficient by encoding
//      raw_len and d_width in 1 byte
typedef struct {
    byte raw[4];
    u8 raw_len;
    u8 display_width;
} CodePoint;

list_def(CodePoint);

#define cp(s)      cp_from_utf8(s, sizeof(s) - 1)
CodePoint cp_from_utf8(byte *s, u8 len);
CodePoint cp_from_raw(byte *raw, u8 raw_len, u8 display_width);
CodePoint cp_from_byte(byte b);
b32 cp_equal(CodePoint a, CodePoint b);

#define ctrl(x) cp_from_byte((x) & 0x1F)

typedef struct {
    u8 r, g, b;
} RGB;

enum {
    EFFECT_FG               = 1 << 0,
    EFFECT_BG               = 1 << 1,
    EFFECT_BOLD             = 1 << 2,
    EFFECT_DIM              = 1 << 3,
    EFFECT_ITALIC           = 1 << 4,
    EFFECT_UNDERLINE        = 1 << 5,
    EFFECT_INVERSE          = 1 << 6,
    EFFECT_STRIKETHROUGH    = 1 << 7,
};

typedef struct {
    RGB fg;
    RGB bg;
    u8 flags;
} Effect;

b32 effect_equal(Effect a, Effect b);

#define CELL_REGULAR        0x00
#define CELL_CONTINUATION   0x01
#define CELL_WIDE_LEAD      0x02

typedef struct {
    CodePoint cp;
    u8 flags;
    Effect effect;
} Cell;

Cell cell(CodePoint cp, Effect e);
Cell cell_cont(Effect e);
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
    b32 handled;
    union {
        struct {
            i32 x, y;
            b32 pressed;
        } mouse;
        CodePoint parsed_cp;
        TermKey term_key;
    };
} Event;

b32 is_event(EventType e);
i32 get_mouse_x();
i32 get_mouse_y();
b32 is_mouse_pressed();
b32 is_mouse_released();
b32 is_term_key(TermKey k);
b32 is_codepoint(CodePoint cp);
b32 is_event_consumed();
void event_consume();
CodePoint get_codepoint();

typedef struct {
    i32 x, y, w, h;
} Rectangle;

b32 point_in_rect(i32 x, i32 y, Rectangle r);
Rectangle rect_intersect(Rectangle a, Rectangle b);
Rectangle rect_union(Rectangle a, Rectangle b);

typedef struct {
    Rectangle rect;
} Clip;

void clip_push(i32 x, i32 y, i32 w, i32 h);
void clip_push_rect(Rectangle r);
Clip clip_pop();
Clip clip_peek();

void init_terminal();
void set_max_timeout_ms(i32 timeout);
void begin_frame();
void end_frame();
u64 get_delta_time();
u32 get_terminal_width();
u32 get_terminal_height();

typedef u32 Unicode;
u8 unicode_width(Unicode ch);

void put_cp_(i32 x, i32 y, CodePoint cp, Effect e);
#define put_cp(...)                 putx_(__VA_ARGS__, put2_, put1_)(__VA_ARGS__)
#define putx_(a, b, c, d, e, ...)   e
#define put1_(x, y, cp)             put_cp_(x, y, cp, (Effect) {0})
#define put2_(x, y, cp, ef)         put_cp_(x, y, cp, ef)

void put_str_(i32 x, i32 y, byte *str, usize len, Effect e);
#define put_str(...)                 putstrx_(__VA_ARGS__, putstr2_, putstr1_)(__VA_ARGS__)
#define putstrx_(a, b, c, d, e, f, ...) f
#define putstr1_(x, y, s, len)       put_str_(x, y, s, len, (Effect){0})
#define putstr2_(x, y, s, len, ef)   put_str_(x, y, s, len, ef)

void draw_line_(i32 x0, i32 y0, i32 x1, i32 y1, CodePoint cp, Effect e);
#define draw_line(...)               drawlinex_(__VA_ARGS__, drawline2_, drawline1_)(__VA_ARGS__)
#define drawlinex_(a,b,c,d,e,f,g,...) g
#define drawline1_(x0,y0,x1,y1,cp)   draw_line_(x0,y0,x1,y1,cp,(Effect){0})
#define drawline2_(x0,y0,x1,y1,cp,ef) draw_line_(x0,y0,x1,y1,cp,ef)

void draw_box_(Rectangle r, Effect e);
#define draw_box(...)                drawboxx_(__VA_ARGS__, drawbox2_, drawbox1_)(__VA_ARGS__)
#define drawboxx_(a,b,c,...)         c
#define drawbox1_(r)                 draw_box_(r, (Effect){0})
#define drawbox2_(r, ef)             draw_box_(r, ef)

void fill_box(Rectangle r, Effect e);

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

void debug(i32 x, i32 y, byte *fmt, ...);

#define MAX(a, b)           ((a) > (b) ? (a) : (b))
#define MIN(a, b)           ((a) < (b) ? (a) : (b))
#define CLAMP(v, min, max)  MIN(max, MAX(v, min))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#endif //LIBTUI_RENDERER_INCLUDE

#ifdef LIBTUI_RENDERER_IMPL

#include <unistd.h>
#include <termios.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

list_def(Cell);
list_def(Clip);
list_def(byte);

struct {
    struct termios orig_term;
    Unix_Pipe pipe;
    i32 timeout;
    Event event;
    u64 saved_time, dt;
    List(Cell) frontbuffer;
    List(Cell) backbuffer;
    List(byte) frame_cmds;
    Arena tmp;
    List(Clip) clips;
    u32 width, height;
} Terminal = {0};

u64 get_delta_time() { return Terminal.dt; }
u32 get_terminal_width() { return Terminal.width; }
u32 get_terminal_height() { return Terminal.height; }
b32 is_event(EventType e) { return Terminal.event.type == e; }
i32 get_mouse_x() { return Terminal.event.mouse.x; }
i32 get_mouse_y() { return Terminal.event.mouse.y; }
b32 is_mouse_pressed() { return Terminal.event.mouse.pressed; }
b32 is_mouse_released() { return !is_mouse_pressed(); }
b32 is_term_key(TermKey k) { return Terminal.event.term_key == k && Terminal.event.type == ETermKey; }
b32 is_codepoint(CodePoint cp) { return cp_equal(cp, Terminal.event.parsed_cp); }
CodePoint get_codepoint() { return Terminal.event.parsed_cp; }

b32 is_event_consumed() { return Terminal.event.handled; }
void event_consume() { Terminal.event.handled = true; }
b32 is_mouse_event() { return Terminal.event.type == EScrollUp ||
                              Terminal.event.type == EScrollDown ||
                              Terminal.event.type == EMouseDrag || 
                              Terminal.event.type == EMouseLeft ||
                              Terminal.event.type == EMouseMiddle ||
                              Terminal.event.type == EMouseRight; }

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
void emit_absolute_cursor_move(List(byte) *a, u32 row, u32 col);
void render();
void update_root_scope();

b32 try_parse_mouse(byte *str, isize n, Event *e);
b32 try_parse_term_key(byte *str, isize n, Event *e);
b32 try_parse_text(byte *str, isize n, Event *e);
void fix_wide_char(i32 x, i32 y);
void emit_cells(List(byte) *out, Cell *cells, usize start, usize len);
void emit_effect(List(byte) *out, Effect e);
void reset_effect(List(byte) *out);

CodePoint utf8_next(byte **start, byte *end);

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
    //TODO: test propagating unhandled events to the terminal emulator.
    //      maybe it will handle them?

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

    list_resize(&Terminal.backbuffer, Terminal.width * Terminal.height);
    list_resize(&Terminal.frontbuffer, Terminal.width * Terminal.height);
    list_resize(&Terminal.frame_cmds, Terminal.width * Terminal.height);

    // manually add the terminal scope
    Rectangle r = {.w = Terminal.width, .h = Terminal.height};
    list_append(&Terminal.clips, (Clip) {.rect = r});

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
    Terminal.clips.items[0] = (Clip) {.rect = r};
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

    list_free(Terminal.backbuffer);
    list_free(Terminal.frontbuffer);
    list_free(Terminal.frame_cmds);
    list_free(Terminal.clips);

    arena_destroy(Terminal.tmp);
}

void handle_sigwinch(i32 signo) {
    update_screen_dimensions();

    u32 new_size = Terminal.width * Terminal.height;
    list_resize(&Terminal.backbuffer, new_size);
    list_resize(&Terminal.frontbuffer, new_size);

    update_root_scope();

    // trigger full redraw
    for (usize i = 0; i < Terminal.frontbuffer.count; ++i)
        Terminal.frontbuffer.items[i] = cell(cp_from_byte(0xFF), (Effect) {0});

    write(Terminal.pipe.write_fd, &signo, sizeof signo);
}

void set_max_timeout_ms(i32 timeout) { Terminal.timeout = timeout; }

void begin_frame() {
    save_timestamp();

    arena_clear(&Terminal.tmp);
    list_clear(&Terminal.frame_cmds);
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
    Cell *back_items = Terminal.backbuffer.items;
    Cell *front_items = Terminal.frontbuffer.items;
    u32 screen_w = Terminal.width;

    for (u32 row = 0; row < Terminal.height; row++) {
        usize row_start = row * screen_w;
        usize row_end   = row_start + screen_w;

        usize pos = row_start;
        while (pos < row_end) {
            if (cell_equal(back_items[pos], front_items[pos])) {
                pos++;
                continue;
            }

            usize run_start = pos;
            Effect run_effect = back_items[run_start].effect;
            while (pos < row_end && 
                !cell_equal(back_items[pos], front_items[pos]) &&
                effect_equal(back_items[pos].effect, run_effect)) {
                pos++;
            }

            usize run_len = pos - run_start;
            u32 new_row = run_start / screen_w;
            u32 new_col = run_start % screen_w;
            emit_absolute_cursor_move(&Terminal.frame_cmds, new_row, new_col);
            emit_cells(&Terminal.frame_cmds, back_items, run_start, run_len);

            memcpy(
                front_items + run_start,
                back_items + run_start,
                run_len * sizeof(Cell)
            );
        }
    }
}

void emit_cells(List(byte) *out, Cell *cells, usize start, usize len) {
    emit_effect(out, cells[start].effect);

    for (usize i = 0; i < len; i++) {
        Cell c = cells[start + i];
        if (c.flags & CELL_CONTINUATION) continue;

        list_append_many(out, c.cp.raw, c.cp.raw_len);
    }

    reset_effect(out);
}

void emit_effect(List(byte) *out, Effect e) {
    if (e.flags == 0) return;

    Stream s = arena_stream_start(&Terminal.tmp, 64);
    stream_fmt(&s, "\33[");

    if (e.flags & EFFECT_BOLD)          stream_fmt(&s, "1;");
    if (e.flags & EFFECT_DIM)           stream_fmt(&s, "2;");
    if (e.flags & EFFECT_ITALIC)        stream_fmt(&s, "3;");
    if (e.flags & EFFECT_UNDERLINE)     stream_fmt(&s, "4;");
    if (e.flags & EFFECT_INVERSE)       stream_fmt(&s, "7;");
    if (e.flags & EFFECT_STRIKETHROUGH) stream_fmt(&s, "9;");

    if (e.flags & EFFECT_FG)
        stream_fmt(&s, "38;2;%u;%u;%u;", e.fg.r, e.fg.g, e.fg.b);
    if (e.flags & EFFECT_BG)
        stream_fmt(&s, "48;2;%u;%u;%u;", e.bg.r, e.bg.g, e.bg.b);

    // replace ';' with 'm'
    *(s.p - 1) = 'm';
    s8 result = stream_end(s);
    list_append_many(out, result.s, result.len);
}

void reset_effect(List(byte) *out) {
    s8 result = s8("\33[0m");
    list_append_many(out, result.s, result.len);
}

void emit_absolute_cursor_move(List(byte) *a, u32 row, u32 col) {
    Stream s = arena_stream_start(&Terminal.tmp, 64);
    stream_fmt(&s, "\33[%u;%uH", row + 1, col + 1);
    s8 result = stream_end(s);

    list_append_many(a, result.s, result.len);
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

    // special case
    #define DELETE 127
    if (n == 1 && *str == DELETE) {
        e->type = ETermKey;
        e->term_key = TERMKEY_BACKSPACE;
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
        e->type = ECodePoint;
        e->parsed_cp = utf8_next(&str, str + n);
        return true;
    }

    return false;
}

CodePoint utf8_next(byte **p, byte *end) {
    byte *s = *p;

    if (s >= end) {
        return UTF8_REPLACEMENT;
    }

    u8 first = s[0];
    if (first < 0x80) {
        *p += 1;
        return cp_from_byte(first);
    }

    usize len = 0;
    Unicode value = 0;

    if ((first & 0xE0) == 0xC0) {
        len = 2;
        value = first & 0x1F;
    } else if ((first & 0xF0) == 0xE0) {
        len = 3;
        value = first & 0x0F;
    } else if ((first & 0xF8) == 0xF0) {
        len = 4;
        value = first & 0x07;
    } else {
        *p += 1;
        return UTF8_REPLACEMENT;
    }

    if (s + len > end) {
        *p = end; // consume rest
        return UTF8_REPLACEMENT;
    }

    for (usize i = 1; i < len; i++) {
        if ((s[i] & 0xC0) != 0x80) {
            *p += i;
            return UTF8_REPLACEMENT;
        }
        value = (value << 6) | (s[i] & 0x3F);
    }

    CodePoint cp = cp_from_raw(s, len, unicode_width(value));
    *p += len;
    return cp;
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
    return utf8_next(&s, s + len);
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

Cell cell(CodePoint cp, Effect e) { return (Cell) { .cp = cp, .effect = e }; }
Cell cell_lead(CodePoint cp, Effect e) { return (Cell) { .cp = cp, .flags = CELL_WIDE_LEAD, .effect = e }; }
Cell cell_cont(Effect e) { return (Cell) { .flags = CELL_CONTINUATION, .effect = e }; }
Cell cell_empty() { return (Cell) { .cp = cp_from_byte(' ') }; }
b32 cell_equal(Cell a, Cell b) { return memcmp(&a, &b, sizeof a) == 0; }

b32 effect_equal(Effect a, Effect b) { return memcmp(&a, &b, sizeof a) == 0; }

void put_str_(i32 x, i32 y, byte *s, usize len, Effect e) {
    byte *p = s;
    byte *end = s + len;

    while (p < end) {
        CodePoint cp = utf8_next(&p, end);
        put_cp(x, y, cp, e);
        x += cp.display_width;
    }
}

void put_cp_(i32 x, i32 y, CodePoint cp, Effect e) {
    Clip parent = clip_peek();

    if (x < 0 || y < 0) return;
    if (!point_in_rect(x, y, parent.rect)) return;

    u32 w = Terminal.width;
    Cell *cells = Terminal.backbuffer.items;

    if (cp.display_width == 1) {
        fix_wide_char(x, y);
        cells[x + y * w] = cell(cp, e);
        return;
    }

    if (cp.display_width == 2) {
        if ((u32)x + 1 >= w) return; // cannot fit

        fix_wide_char(x, y);
        fix_wide_char(x + 1, y);

        cells[x + y * w] = cell_lead(cp, e);
        cells[(x + 1) + y * w] = cell_cont(e);
        return;
    }

    // ignore other widths
    assert(false && "a codepoint with invalid width");
}

void put_effect(i32 x, i32 y, Effect e) {
    Clip parent = clip_peek();

    if (x < 0 || y < 0) return;
    if (!point_in_rect(x, y, parent.rect)) return;

    u32 w = Terminal.width;
    Cell *cells = Terminal.backbuffer.items;
    Cell *cur = &cells[x + y * w];
    
    cur->effect = e;
    //TODO: this causes infinite recursion
    if (cur->flags & CELL_WIDE_LEAD) {
        put_effect(x + 1, y, e);
    } else if (cur->flags & CELL_CONTINUATION) {
        put_effect(x - 1, y, e);
    }
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

void clip_push(i32 x, i32 y, i32 w, i32 h) {
    Rectangle r = {x,y,w,h};
    clip_push_rect(r);
}

void clip_push_rect(Rectangle r) {
    Clip parent = clip_peek();
    Rectangle clipped = rect_intersect(parent.rect, r);
    list_append(&Terminal.clips, (Clip){.rect = clipped});
}

Clip clip_pop() { 
    return list_pop(&Terminal.clips);
}

Clip clip_peek() {
    return list_last(&Terminal.clips);
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

void put_cp_debug(i32 x, i32 y, CodePoint cp) {
    u32 w = Terminal.width;
    Cell *cells = Terminal.backbuffer.items;
    cells[x + y * w] = cell(cp, (Effect) {0});
}

void debug(i32 x, i32 y, byte *fmt, ...) {
    Stream s = arena_stream_start(&Terminal.tmp, 256);

    va_list args;
    va_start(args, fmt);
    s.p = vfmt(s.p, s.end, fmt, args);
    va_end(args);

    s8 str = stream_end(s);
    for (usize i = 0; i < str.len; i++)
        put_cp_debug(x + i, y, cp_from_byte(str.s[i]));
}

void draw_line_(i32 x0, i32 y0, i32 x1, i32 y1, CodePoint cp, Effect e) {
    if (x0 == x1) { // vertical
        if (y1 < y0) {
            i32 tmp = y0;
            y0 = y1;
            y1 = tmp;
        }

        for (i32 y = y0; y <= y1; y++) {
            put_cp(x0, y, cp, e);
        }
    }
    else if (y0 == y1) { // horizontal
        if (x1 < x0) {
            i32 tmp = x0;
            x0 = x1;
            x1 = tmp;
        }

        for (i32 x = x0; x <= x1; x++) {
            put_cp(x, y0, cp, e);
        }
    }
}

void draw_box_(Rectangle r, Effect e) {
    if (r.w < 2 || r.h < 2) return;

    i32 x0 = r.x;
    i32 y0 = r.y;
    i32 x1 = r.x + r.w - 1;
    i32 y1 = r.y + r.h - 1;

    put_cp(x0, y0, cp("┌"), e);
    put_cp(x1, y0, cp("┐"), e);
    put_cp(x0, y1, cp("└"), e);
    put_cp(x1, y1, cp("┘"), e);

    draw_line(x0 + 1, y0, x1 - 1, y0, cp("─"), e);
    draw_line(x0 + 1, y1, x1 - 1, y1, cp("─"), e);
    draw_line(x0, y0 + 1, x0, y1 - 1, cp("│"), e);
    draw_line(x1, y0 + 1, x1, y1 - 1, cp("│"), e);
}

void fill_box(Rectangle r, Effect e) {
    for (i32 j = 0; j < r.h; ++j) {
        for (i32 i = 0; i < r.w; ++i) {
            put_effect(r.x + i, r.y + j, e);
        }
    }
}

#endif //LIBTUI_RENDERER_IMPL