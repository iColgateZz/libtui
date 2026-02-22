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

typedef struct {
    byte raw[4];
    u8 raw_len;
    u8 display_width;
} CodePoint;

CodePoint cp_new(byte *raw, u8 raw_len, u8 display_width) {
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

typedef enum {
    ENone,
    EKey,
    EText,
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
    u32 x, y;
    b32 mouse_pressed;
    byte buf[32];
    CodePoint parsed_cp;
    Key key;
} Event;

EventType get_event_type();
b32 is_event_type(EventType e);
u32 get_mouse_x();
u32 get_mouse_y();
b32 is_mouse_pressed();
b32 is_mouse_released();
Event get_event();
Key get_key();
b32 is_key_pressed(Key k);
b32 is_codepoint(CodePoint cp);

typedef struct {
    u32 x, y, w, h;
} Rectangle;

b32 point_in_rect(u32 x, u32 y, Rectangle r);
Rectangle rect_intersect(Rectangle a, Rectangle b);
Rectangle rect_union(Rectangle a, Rectangle b);

typedef struct {
    Arena arena;
    byte *items;
    usize count;
    usize capacity;
    usize item_size;
} Array;

Array array_init(usize reserve_size, usize item_size);
void  array_resize(Array *a, usize new_size);
void  array_extend_to(Array *a, usize new_capacity);
void  array_append(Array *a, void *item, usize n);
void *array_get(Array *a, usize i);
void  array_destroy(Array a);

void init_terminal();
void set_max_timeout_ms(u32 timeout);
void begin_frame();
void end_frame();
u64 get_delta_time();
u32 get_terminal_width();
u32 get_terminal_height();

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

struct {
    struct termios orig_term;
    Unix_Pipe pipe;
    u32 timeout;
    Event event;
    u64 saved_time, dt;
    Array frontbuffer;
    Array backbuffer;
    Array frame_cmds;
    Array scopes;
    u32 width, height;
} Terminal = {0};

u64 get_delta_time() { return Terminal.dt; }
u32 get_terminal_width() { return Terminal.width; }
u32 get_terminal_height() { return Terminal.height; }
EventType get_event_type() { return Terminal.event.type; }
b32 is_event_type(EventType e) { return Terminal.event.type == e; }
u32 get_mouse_x() { return Terminal.event.x; }
u32 get_mouse_y() { return Terminal.event.y; }
b32 is_mouse_pressed() { return Terminal.event.mouse_pressed; }
b32 is_mouse_released() { return !is_mouse_pressed(); }
Event get_event() { return Terminal.event; }
Key get_key() { return Terminal.event.key; }
b32 is_key_pressed(Key k) { return Terminal.event.key == k 
                            && Terminal.event.type == EKey; }
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
void generate_absolute_cursor_move(Array *a, u32 row, u32 col);
void generate_relative_cursor_move(Array *a, u32 step);
usize u32_to_ascii(byte *dst, u32 value);
void pop_scope();
Rectangle peek_scope();
void push_scope(u32 x, u32 y, u32 w, u32 h);
void render();
void update_terminal_scope();

// TODO: remove this array impl and use da_append
//       and its friends
// TODO: utf-8 handling

Array array_init(usize reserve_size, usize item_size) {
    Arena arena = arena_init(GB(16));

    byte *arr = arena_push(&arena, byte, reserve_size * item_size);
    assert(arr);
    memset(arr, 0, reserve_size * item_size);

    return (Array) {
        .arena = arena,
        .items = arr,
        .count = 0,
        .capacity = arena.committed_size,
        .item_size = item_size
    };
}

void array_resize(Array *a, usize new_size) {
    arena_clear(&a->arena);
    a->items = arena_push(&a->arena, byte, new_size * a->item_size);
    assert(a->items);
    a->capacity = a->arena.committed_size;
}

void array_extend_to(Array *a, usize new_capacity) {
    if (new_capacity * a->item_size >= a->capacity) {
        array_resize(a, new_capacity);
    }
}

void array_append(Array *a, void *item, usize n) {
    if ((a->count + n) * a->item_size >= a->capacity) {
        array_resize(a, a->count + n);
    }

    memcpy(a->items + a->count * a->item_size, item, n * a->item_size);
    a->count += n;
}

void *array_get(Array *a, usize i) {
    return a->items + i * a->item_size;
}

void array_destroy(Array a) { arena_destroy(a.arena); }

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

    Terminal.backbuffer  = array_init(Terminal.width * Terminal.height, sizeof(CodePoint));
    Terminal.backbuffer.count = Terminal.width * Terminal.height;

    Terminal.frontbuffer = array_init(Terminal.width * Terminal.height, sizeof(CodePoint));
    Terminal.frontbuffer.count = Terminal.width * Terminal.height;

    Terminal.frame_cmds  = array_init(Terminal.width * Terminal.height, sizeof(byte));
    Terminal.scopes      = array_init(256, sizeof(Rectangle));

    // manually add the terminal scope
    Terminal.scopes.count = 1;
    update_terminal_scope();
}

void update_screen_dimensions() {
    struct winsize ws = {0};
    assert(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0);

    Terminal.width = ws.ws_col;
    Terminal.height = ws.ws_row;
}

void update_terminal_scope() {
    Rectangle *r = (Rectangle *)Terminal.scopes.items;
    *r = (Rectangle) {
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

    array_destroy(Terminal.backbuffer);
    array_destroy(Terminal.frontbuffer);
    array_destroy(Terminal.frame_cmds);
    array_destroy(Terminal.scopes);
}

void handle_sigwinch(i32 signo) {
    update_screen_dimensions();

    u32 new_size = Terminal.width * Terminal.height;
    array_extend_to(&Terminal.backbuffer, new_size);
    array_extend_to(&Terminal.frontbuffer, new_size);
    Terminal.backbuffer.count  = new_size;
    Terminal.frontbuffer.count = new_size;

    update_terminal_scope();

    // trigger full redraw
    CodePoint *cps = (CodePoint *)Terminal.frontbuffer.items;
    for (usize i = 0; i < Terminal.frontbuffer.count; ++i) {
        cps[i] = cp_from_byte(0xFF);
    }

    write(Terminal.pipe.write_fd, &signo, sizeof signo);
}

void set_max_timeout_ms(u32 timeout) { Terminal.timeout = timeout; }

void begin_frame() {
    save_timestamp();
    CodePoint *cps = (CodePoint *)Terminal.backbuffer.items;
    for (usize i = 0; i < Terminal.backbuffer.count; ++i) {
        cps[i] = cp_from_byte(' ');
    }
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

        CodePoint *back_items = (CodePoint *)Terminal.backbuffer.items;
        CodePoint *front_items = (CodePoint *)Terminal.frontbuffer.items;

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
                    array_append(&Terminal.frame_cmds, back_items[run_start + i].raw, back_items[run_start + i].raw_len);
                }
            } else if (gap <= GAP_THRESHOLD) {
                // instead of emitting cursor move, just copy the bytes
                // that are the same in both buffers
                for (usize i = 0; i < run_len + gap; ++i) {
                    array_append(&Terminal.frame_cmds, back_items[run_start - gap + i].raw, back_items[run_start - gap + i].raw_len);
                }
                gap = 0;
            } else {
                // I know it is the same row
                u32 new_col = run_start % screen_w;
                generate_relative_cursor_move(&Terminal.frame_cmds, new_col - cursor.x);
                for (usize i = 0; i < run_len; ++i) {
                    array_append(&Terminal.frame_cmds, back_items[run_start + i].raw, back_items[run_start + i].raw_len);
                }
            }

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

void generate_absolute_cursor_move(Array *a, u32 row, u32 col) {
    byte tmp[64];
    byte *p = tmp;

    *p++ = '\33';
    *p++ = '[';
    p += u32_to_ascii(p, row + 1);
    *p++ = ';';
    p += u32_to_ascii(p, col + 1);
    *p++ = 'H';

    array_append(a, tmp, (usize)(p - tmp));
}

void generate_relative_cursor_move(Array *a, u32 step) {
    byte tmp[64];
    byte *p = tmp;

    *p++ = '\33';
    *p++ = '[';
    p += u32_to_ascii(p, step);
    *p++ = 'C';

    array_append(a, tmp, (usize)(p - tmp));
}

usize u32_to_ascii(byte *dst, u32 value) {
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

static struct {byte str[4]; Key k;} key_table[] = {
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

void parse_event(Event *e, isize n) {
    byte *str = e->buf;

    if (n == 1 && str[0] != ESCAPE_KEY) { // regular key
        // write_strf("%ld: '%.*s'\r\n", n, (i32)n, str);
        
        #define DELETE 127
        if (str[0] == DELETE) {
            e->type = EKey;
            e->key = BACKSPACE_KEY;
        } else {
            // decode ut8 later
            e->type = EText;
            e->parsed_cp = cp_from_byte(str[0]);
        }

        return;
    }

    if (str[0] != ESCAPE_KEY && 1 <= n && n <= 4) { // utf-8
        e->type = EText;
        e->parsed_cp = cp_new(str, n, 1);
        return;
    }

    if (n >= 9 && memcmp(str, "\33[<", 3) == EXIT_SUCCESS) {
        // write_strf("%ld: '%.*s'\r\n", n, (i32)n - 3, str + 3);
        u32 btn = strtol(str + 3, &str, 10);
        e->x    = strtol(str + 1, &str, 10) - 1;
        e->y    = strtol(str + 1, &str, 10) - 1;
        e->mouse_pressed = str[0] == 'M' ? true : false;
        // write_strf("btn: %d, x: %d, y: %d\r\n", btn, e->x, e->y);
        switch (btn) {
            case 0:  e->type = EMouseLeft;   break;
            case 1:  e->type = EMouseMiddle; break;
            case 2:  e->type = EMouseRight;  break;
            case 32: e->type = EMouseDrag;   break;
            case 64: e->type = EScrollUp;    break;
            case 65: e->type = EScrollDown;  break;
            default: e->type = ENone;
        }

        return;
    }

    if ((n == 3 || n == 4) && str[0] == ESCAPE_KEY) { // longer escaped sequence
        for (usize i = 0; i < ARRAY_SIZE(key_table); i++) {
            if (memcmp(str + 1, key_table[i].str, n - 1) == EXIT_SUCCESS) {
                e->type = EKey;
                e->key  = key_table[i].k;
                return;
            }
        }
    }

    e->type = ENone;
    return;
}

void put_char(u32 x, u32 y, byte c) {
    Rectangle parent = peek_scope();
    if (!point_in_rect(x, y, parent)) return;

    CodePoint *back_items = (CodePoint *)Terminal.backbuffer.items;
    back_items[x + y * Terminal.width] = cp_from_byte(c);
}

// void put_str(u32 x, u32 y, byte *str, usize len) {
//     Rectangle parent = peek_scope();
//     if (!point_in_rect(x, y, parent)) return;

//     usize copy_len = MIN(len, parent.x + parent.w - x);
//     memcpy(Terminal.backbuffer.items + x + y * Terminal.width, str, copy_len);
// }

void push_scope(u32 x, u32 y, u32 w, u32 h) {
    Rectangle parent = peek_scope();
    Rectangle clipped = rect_intersect(parent, (Rectangle) {x, y, w, h});
    array_append(&Terminal.scopes, &clipped, 1);
}

void pop_scope() { 
    Terminal.scopes.count--;
    assert(Terminal.scopes.count >= 1);
}

Rectangle peek_scope() {
    return *(Rectangle *)array_get(&Terminal.scopes, Terminal.scopes.count - 1);
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

#endif //LIBTUI_IMPL
