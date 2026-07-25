#include "brenda_internal.h"

#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/poll.h>
#include <time.h>

static State state = {0};

u64 brenda_frame_get_delta_time(void) { return state.delta_time; }
u32 brenda_terminal_get_width(void) { return state.width; }
u32 brenda_terminal_get_height(void) { return state.height; }
Brenda_EventSlice brenda_events_get(void) {
    return (Brenda_EventSlice) {
        .items = state.events.items,
        .count = state.events.count,
    };
}

#define write_string(string) output_write(string, sizeof(string) - 1)

static void output_write(byte *text, usize length) { write(STDOUT_FILENO, text, length); }

void brenda_terminal_init(void) {
    assert(tcgetattr(STDIN_FILENO, &state.original_terminal) == 0);
    
    struct termios raw = state.original_terminal;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    assert(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0);
    //TODO: maybe add something to deal with the cursor on the user side
    //      so that the user can manipulate the cursor position maybe

    write_string("\33[?2004l");                 // disable bracketed paste mode
    write_string("\33[?1049h");                 // use alternate buffer
    write_string("\33[?25l");                   // hide cursor
    write_string("\33[?1000h");                 // enable mouse press/release
    write_string("\33[?1002h");                 // enable mouse press/release + drag
    write_string("\33[?1003h");                 // enable mouse press/release + drag + hover
    write_string("\33[?1006h");                 // use mouse sgr protocol
    write_string("\33[0m");                     // reset text attributes
    write_string("\33[2J");                     // clear screen
    write_string("\33[H");                      // move cursor to home position

    screen_dimensions_update();
    atexit(terminal_restore);
    assert(pipe_open(&state.pipe));

    struct sigaction sa = {0};
    sa.sa_handler = signal_winch_handle;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sa, NULL);

    list_resize(&state.back_buffer, state.width * state.height);
    list_resize(&state.front_buffer, state.width * state.height);
    list_resize(&state.frame_commands, state.width * state.height);

    // manually add the terminal scope
    Brenda_Rectangle r = {.w = state.width, .h = state.height};
    list_append(&state.clips, r);

    state.tmp = arena_init(MB(16));
}

static void screen_dimensions_update(void) {
    struct winsize ws = {0};
    assert(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0);

    state.width = ws.ws_col;
    state.height = ws.ws_row;
}

static void root_clip_update(void) {
    Brenda_Rectangle r = {.w = state.width, .h = state.height};
    state.clips.items[0] = r;
}

static void terminal_restore(void) {
    assert(tcsetattr(STDIN_FILENO, TCSAFLUSH, &state.original_terminal) == 0);

    write_string("\33[?1000l");                     // disable mouse
    write_string("\33[?1002l");                     // disable mouse
    write_string("\33[?1003l");                     // disable mouse
    write_string("\33[0m");                         // reset text attributes
    write_string("\33[?25h");                       // show cursor
    write_string("\33[?1049l");                     // exit alternate buffer

    fd_close(state.pipe.read_fd);
    fd_close(state.pipe.write_fd);

    list_free(state.back_buffer);
    list_free(state.front_buffer);
    list_free(state.frame_commands);
    list_free(state.clips);
    list_free(state.events);
    list_free(state.input_bytes);

    arena_destroy(state.tmp);
}

static void signal_winch_handle(i32 signo) {
    screen_dimensions_update();

    u32 new_size = state.width * state.height;
    list_resize(&state.back_buffer, new_size);
    list_resize(&state.front_buffer, new_size);

    root_clip_update();

    // trigger full redraw
    for (isize i = 0; i < state.front_buffer.count; ++i)
        state.front_buffer.items[i] = cell(text_unit_from_byte(0xFF), (Brenda_Effect) {0});

    write(state.pipe.write_fd, &signo, sizeof signo);
}

void brenda_terminal_set_fps(i32 fps) {
    state.frame_interval_ns = fps <= 0 ? -1 : 1000000000ull / fps;
}

void brenda_frame_begin(void) {
    timestamp_save();

    arena_clear(&state.tmp);
    list_clear(&state.frame_commands);
    list_clear(&state.events);
    for (isize i = 0; i < state.back_buffer.count; ++i)
        state.back_buffer.items[i] = cell_empty();

    if (state.frame_interval_ns <= 0) {
        events_handle_available(-1);
        return;
    }

    i64 deadline = time_get_ns() + state.frame_interval_ns;
    events_poll_until(deadline);
}

static void timestamp_save(void) { state.saved_time = time_get_ms(); }
static void delta_time_calculate(void) { state.delta_time = time_get_ms() - state.saved_time; }

static i64 time_get_ns(void) {
    struct timespec ts = {0};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

static i64 time_get_ms(void) {
    return time_get_ns() / 1000000ull;
}

void brenda_frame_end(void) {
    frame_render();
    output_write(state.frame_commands.items, state.frame_commands.count);
    delta_time_calculate();
}

//TODO: maybe hash each row and compare hashes?
static void frame_render(void) {
    Cell *back_items = state.back_buffer.items;
    Cell *front_items = state.front_buffer.items;
    u32 screen_w = state.width;

    for (u32 row = 0; row < state.height; row++) {
        usize row_start = row * screen_w;
        usize row_end   = row_start + screen_w;

        usize pos = row_start;
        while (pos < row_end) {
            if (cell_equal(back_items[pos], front_items[pos])) {
                pos++;
                continue;
            }

            usize run_start = pos;
            Brenda_Effect run_effect = back_items[run_start].effect;
            while (pos < row_end && 
                !cell_equal(back_items[pos], front_items[pos]) &&
                brenda_effect_equal(back_items[pos].effect, run_effect)) {
                pos++;
            }

            usize run_len = pos - run_start;
            u32 new_row = run_start / screen_w;
            u32 new_col = run_start % screen_w;
            cursor_move_emit(&state.frame_commands, new_row, new_col);
            cells_emit(&state.frame_commands, back_items, run_start, run_len);

            memcpy(
                front_items + run_start,
                back_items + run_start,
                run_len * sizeof(Cell)
            );
        }
    }
}

static void cells_emit(List(byte) *out, Cell *cells, usize start, usize len) {
    effect_emit(out, cells[start].effect);

    for (usize i = 0; i < len; i++) {
        Cell c = cells[start + i];
        if (c.flags & CELL_CONTINUATION) continue;

        list_append_many(out, c.text_unit.utf8, c.text_unit.utf8_length);
    }

    effect_reset(out);
}

static void effect_emit(List(byte) *out, Brenda_Effect e) {
    if (e.flags == 0) return;

    Brenda_Stream s = brenda_stream_start_from_arena(&state.tmp, 64);
    brenda_stream_format(&s, "\33[");

    if (e.flags & BRENDA_EFFECT_BOLD)          brenda_stream_format(&s, "1;");
    if (e.flags & BRENDA_EFFECT_DIM)           brenda_stream_format(&s, "2;");
    if (e.flags & BRENDA_EFFECT_ITALIC)        brenda_stream_format(&s, "3;");
    if (e.flags & BRENDA_EFFECT_UNDERLINE)     brenda_stream_format(&s, "4;");
    if (e.flags & BRENDA_EFFECT_INVERSE)       brenda_stream_format(&s, "7;");
    if (e.flags & BRENDA_EFFECT_STRIKETHROUGH) brenda_stream_format(&s, "9;");

    if (e.flags & BRENDA_EFFECT_FG)
        brenda_stream_format(&s, "38;2;%u;%u;%u;", e.fg.r, e.fg.g, e.fg.b);
    if (e.flags & BRENDA_EFFECT_BG)
        brenda_stream_format(&s, "48;2;%u;%u;%u;", e.bg.r, e.bg.g, e.bg.b);

    // replace ';' with 'm'
    *(s.cursor - 1) = 'm';
    s8 result = brenda_stream_end(s);
    list_append_many(out, result.s, result.len);
}

static void effect_reset(List(byte) *out) {
    s8 result = s8("\33[0m");
    list_append_many(out, result.s, result.len);
}

static void cursor_move_emit(List(byte) *a, u32 row, u32 col) {
    Brenda_Stream s = brenda_stream_start_from_arena(&state.tmp, 64);
    brenda_stream_format(&s, "\33[%u;%uH", row + 1, col + 1);
    s8 result = brenda_stream_end(s);

    list_append_many(a, result.s, result.len);
}

static void events_poll_until(i64 deadline_ns) {
    for (;;) {
        i64 remaining_ns = deadline_ns - time_get_ns();
        if (remaining_ns <= 0) break;
        i32 timeout_ms = (remaining_ns + 999999) / 1000000;
        events_handle_available(timeout_ms);
    }
}

static void events_handle_available(i32 timeout_ms) {
    #define PFD_SIZE 2
    struct pollfd pfd[PFD_SIZE] = {
        {.fd = state.pipe.read_fd, .events = POLLIN},
        {.fd = STDIN_FILENO,       .events = POLLIN},
    };

    i32 rval = poll(pfd, PFD_SIZE, timeout_ms);

    if (rval < 0) {
        if (errno == EAGAIN || errno == EINTR) {
            return;
        }

        assert(false && "call to 'poll' failed");
    }

    else if (rval == 0) {
        return;
    }

    if (pfd[0].revents & POLLIN) { // window resize
        i32 sig;
        read(state.pipe.read_fd, &sig, sizeof sig);
        list_append(&state.events, ((Brenda_Event) {.type = BRENDA_EVENT_WINCH}));
    }

    if (pfd[1].revents & POLLIN) {
        //TODO: Maybe read directly into input_bytes?
        static byte buffer[4096];
        isize n = read(STDIN_FILENO, buffer, sizeof buffer);

        assert(n > 0 && "read non-positive amount of bytes from STDIN");

        list_append_many(&state.input_bytes, buffer, n);
        input_parse_pending();
    }
}

static void input_parse_pending(void) {
    byte *start = state.input_bytes.items;
    byte *p = start;
    byte *end = start + state.input_bytes.count;

    while (p < end) {
        byte *before = p;
        Brenda_Event e = { .type = BRENDA_EVENT_NONE };

        if (*p == BRENDA_TERM_KEY_ESCAPE) {
            if (!escape_parse(&p, end, &e)) break;
        } else if (*p == 127) {
            p++;
            e = (Brenda_Event) {
                .type = BRENDA_EVENT_TERM_KEY,
                .as.term_key = BRENDA_TERM_KEY_BACKSPACE,
            };
        } else {
            if (!text_parse(&p, end, &e)) break;
        }

        assert(p > before);
        if (e.type != BRENDA_EVENT_NONE) list_append(&state.events, e);
    }

    isize consumed = p - start;
    if (consumed == 0) return;

    memmove(
        state.input_bytes.items,
        state.input_bytes.items + consumed,
        (state.input_bytes.count - consumed) * sizeof(*state.input_bytes.items)
    );
    state.input_bytes.count -= consumed;
}

static b32 escape_parse(byte **p, byte *end, Brenda_Event *e) {
    byte *start = *p;
    if (end - start == 1) return false;

    if (start[1] != '[') {
        *p = start + 1;
        return true;
    }

    if (mouse_parse(p, end, e)) return true;
    if (term_key_parse(p, end, e)) return true;

    // Getting rid of unknown sequence
    for (byte *it = start + 2; it < end; ++it) {
        if (0x40 <= *it && *it <= 0x7E) {
            *p = it + 1;
            return true;
        }
    }

    return false;
}

static b32 mouse_parse(byte **p, byte *end, Brenda_Event *e) {
    byte *start = *p;
    isize n = end - start;
    if (n < 9 || memcmp(start, "\33[<", 3) != 0) return false;

    byte *terminator_m = memchr(start, 'm', n);
    byte *terminator_M = memchr(start, 'M', n);
    byte *terminator = MAX(terminator_m, terminator_M);
    if (!terminator) return false;

    byte *cursor        = start;
    u32 btn             = strtol(cursor + 3, &cursor, 10);
    e->as.mouse.x       = strtol(cursor + 1, &cursor, 10) - 1;
    e->as.mouse.y       = strtol(cursor + 1, &cursor, 10) - 1;
    e->as.mouse.pressed = (*cursor == 'M');

    switch (btn) {
        case 0:  e->type = BRENDA_EVENT_MOUSE_LEFT;   break;
        case 1:  e->type = BRENDA_EVENT_MOUSE_MIDDLE; break;
        case 2:  e->type = BRENDA_EVENT_MOUSE_RIGHT;  break;
        case 32: e->type = BRENDA_EVENT_MOUSE_DRAG;   break;
        case 35: e->type = BRENDA_EVENT_MOUSE_MOVE;   break;
        case 64: e->type = BRENDA_EVENT_SCROLL_UP;    break;
        case 65: e->type = BRENDA_EVENT_SCROLL_DOWN;  break;
        default: assert(false && "Unknown mouse event");
    }

    *p = terminator + 1;
    return true;
}

static struct {byte str[4]; Brenda_TermKey k;} term_key_table[] = {
    {"[A" , BRENDA_TERM_KEY_UP},
    {"[B" , BRENDA_TERM_KEY_DOWN},
    {"[C" , BRENDA_TERM_KEY_RIGHT},
    {"[D" , BRENDA_TERM_KEY_LEFT},
    {"[2~", BRENDA_TERM_KEY_INSERT},
    {"[3~", BRENDA_TERM_KEY_DELETE},
    {"[H" , BRENDA_TERM_KEY_HOME},
    {"[4~", BRENDA_TERM_KEY_END},
    {"[5~", BRENDA_TERM_KEY_PAGE_UP},
    {"[6~", BRENDA_TERM_KEY_PAGE_DOWN},
};

static b32 term_key_parse(byte **p, byte *end, Brenda_Event *e) {
    byte *start = *p;
    isize n = end - start;
    if (n < 3) return false;

    for (usize i = 0; i < ARRAY_SIZE(term_key_table); i++) {
        byte *key = term_key_table[i].str;
        isize key_len = strlen(key);
        if (n >= key_len + 1 && memcmp(start + 1, key, key_len) == 0) {
            e->type = BRENDA_EVENT_TERM_KEY;
            e->as.term_key = term_key_table[i].k;
            *p = start + key_len + 1;
            return true;
        }
    }

    return false;
}

static b32 text_parse(byte **p, byte *end, Brenda_Event *e) {
    byte *start = *p;
    u8 expected_length = utf8_expected_length(*start);
    if (end - start < expected_length) return false;

    //TODO: utf8_next also decodes width. It is not needed here.
    TerminalTextUnit text_unit = utf8_next(p, start + expected_length);
    e->type = BRENDA_EVENT_UTF8;
    e->as.utf8.length = text_unit.utf8_length;
    memcpy(e->as.utf8.bytes, text_unit.utf8, text_unit.utf8_length);
    return true;
}

static Cell cell(TerminalTextUnit text_unit, Brenda_Effect effect) {
    return (Cell) { .text_unit = text_unit, .effect = effect };
}

static Cell cell_empty(void) { return (Cell) { .text_unit = text_unit_from_byte(' ') }; }
static b32 cell_equal(Cell a, Cell b) { return memcmp(&a, &b, sizeof a) == 0; }

static inline b32 brenda_effect_equal(Brenda_Effect a, Brenda_Effect b) { return memcmp(&a, &b, sizeof a) == 0; }

static void text_unit_put(i32 x, i32 y, TerminalTextUnit text_unit) {
    Brenda_Rectangle parent = brenda_clip_peek();
    if (!brenda_rectangle_contains_point(parent, x, y)) return;

    u32 w = state.width;
    Cell *cells = state.back_buffer.items;

    if (text_unit.cell_width == 1) {
        wide_character_fix(x, y);
        cells[x + y * w].text_unit = text_unit;
        return;
    }

    if (text_unit.cell_width == 2) {
        if ((u32)x + 1 >= w) return; // cannot fit

        wide_character_fix(x, y);
        wide_character_fix(x + 1, y);

        Cell *lead = &cells[x + y * w];
        lead->text_unit = text_unit;
        lead->flags = CELL_WIDE_LEAD;
        Cell *cont = &cells[(x + 1) + y * w];
        cont->flags = CELL_CONTINUATION;
        return;
    }

    assert(false && "a terminal text unit has an invalid cell width");
}

static void wide_character_fix(i32 x, i32 y) {
    u32 w = state.width;
    Cell *cells = state.back_buffer.items;
    Cell c = cells[x + y * w];

    if ((c.flags & CELL_WIDE_LEAD) && (u32)x + 1 < w) {
        cells[(x + 1) + y * w] = cell_empty();
    }

    if ((c.flags & CELL_CONTINUATION) && (u32)x > 0) {
        cells[(x - 1) + y * w] = cell_empty();
    }
}

void brenda_effect_put(i32 x, i32 y, Brenda_Effect e) {
    Brenda_Rectangle parent = brenda_clip_peek();
    if (!brenda_rectangle_contains_point(parent, x, y)) return;

    u32 w = state.width;
    Cell *cells = state.back_buffer.items;
    Cell *cur = &cells[x + y * w];

    cur->effect = e;
    if ((cur->flags & CELL_WIDE_LEAD) && (u32)x + 1 < w) {
        cells[(x + 1) + y * w].effect = e;
    } else if ((cur->flags & CELL_CONTINUATION) && x > 0) {
        cells[(x - 1) + y * w].effect = e;
    }
}

void brenda_effect_merge(i32 x, i32 y, Brenda_Effect new_effect) {
    Brenda_Rectangle parent = brenda_clip_peek();
    if (!brenda_rectangle_contains_point(parent, x, y)) return;

    u32 w = state.width;
    Cell *cells = state.back_buffer.items;
    Brenda_Effect *cur = &cells[x + y * w].effect;

    // 0 0 | 0
    // 0 1 | 1
    // 1 0 | 1
    // 1 1 | 1

    cur->flags |= new_effect.flags;
    if (new_effect.flags & BRENDA_EFFECT_FG) cur->fg = new_effect.fg;
    if (new_effect.flags & BRENDA_EFFECT_BG) cur->bg = new_effect.bg;
}

i32 brenda_text_measure_width(byte *text, isize length) {
    i32 width = 0;
    byte *cursor = text;
    byte *end = text + length;
    while (cursor < end) width += utf8_next(&cursor, end).cell_width;
    return width;
}

void brenda_text_put(i32 x, i32 y, byte *text, isize length) {
    byte *cursor = text;
    byte *end = text + length;
    while (cursor < end) {
        TerminalTextUnit text_unit = utf8_next(&cursor, end);
        text_unit_put(x, y, text_unit);
        x += text_unit.cell_width;
    }
}

void brenda_clip_push(i32 x, i32 y, i32 w, i32 h) {
    Brenda_Rectangle r = {x,y,w,h};
    brenda_clip_push_rectangle(r);
}

void brenda_clip_push_rectangle(Brenda_Rectangle r) {
    Brenda_Rectangle parent = brenda_clip_peek();
    Brenda_Rectangle clipped = brenda_rectangle_intersect(parent, r);
    list_append(&state.clips, clipped);
}

Brenda_Rectangle brenda_clip_pop(void) { 
    return list_pop(&state.clips);
}

Brenda_Rectangle brenda_clip_peek(void) {
    return list_last(&state.clips);
}

static inline b32 brenda_rectangle_contains_point(Brenda_Rectangle r, i32 x, i32 y) {
    return r.x <= x && x < r.x + r.w 
        && r.y <= y && y < r.y + r.h;
}

static inline Brenda_Rectangle brenda_rectangle_intersect(Brenda_Rectangle a, Brenda_Rectangle b) {
    i32 x1 = MAX(a.x, b.x);
    i32 y1 = MAX(a.y, b.y);
    i32 x2 = MIN(a.x + a.w, b.x + b.w);
    i32 y2 = MIN(a.y + a.h, b.y + b.h);

    if (x2 <= x1 || y2 <= y1) {
        return (Brenda_Rectangle){0,0,0,0}; // fully clipped
    }

    return (Brenda_Rectangle){ x1, y1, x2 - x1, y2 - y1 };
}

static inline Brenda_Rectangle brenda_rectangle_union(Brenda_Rectangle a, Brenda_Rectangle b) {
    i32 left   = MIN(a.x, b.x);
    i32 top    = MIN(a.y, b.y);
    i32 right  = MAX(a.x + a.w, b.x + b.w);
    i32 bottom = MAX(a.y + a.h, b.y + b.h);

    return (Brenda_Rectangle) {
        .x = left,
        .y = top,
        .w = right - left,
        .h = bottom - top
    };
}

byte *brenda_format(byte *p, byte *end, byte *f, ...) {
    va_list args;
    va_start(args, f);
    p = format_variadic(p, end, f, args);
    va_end(args);
    return p;
}

// Behaviour is similar to snprintf, in that it does not
// write beyond the *end pointer, but the returned pointer
// may point somewhere beyond the *end. 
static byte *format_variadic(byte *p, byte *end, byte *f, va_list args) {
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
                i32 v = va_arg(args, i32);

                if (v < 0) {
                    if (p < end) *p = '-';
                    p++;

                    u64 u = (u64)(-v);
                    p = format_uint(p, end, u, 10);
                } else {
                    p = format_uint(p, end, (u64)v, 10);
                }

            } break;

            case 'u': {
                u64 v = va_arg(args, u64);
                p = format_uint(p, end, v, 10);
            } break;

            case 'x': {
                u64 v = va_arg(args, u64);
                p = format_uint(p, end, v, 16);
            } break;

            case 's': {
                byte *s = va_arg(args, byte *);
                p = format_cstring(p, end, s);
            } break;

            case 'S': {
                s8 s = va_arg(args, s8);
                p = format_string(p, end, s);
            } break;

            case '%': {
                if (p < end) *p = '%';
                p++;
            } break;
        }
    }

    return p;
}

static byte *format_uint(byte *p, byte *end, u64 v, u8 base) {
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

static byte *format_cstring(byte *p, byte *end, byte *s) {
    while (*s) {
        if (p < end) *p = *s;
        p++;
        s++;
    }
    return p;
}

static byte *format_string(byte *p, byte *end, s8 s) {
    for (isize i = 0; i < s.len; i++) {
        if (p < end) *p = s.s[i];
        p++;
    }
    return p;
}

Brenda_Stream brenda_stream_start(byte *buffer, usize size) {
    return (Brenda_Stream) {
        .start = buffer,
        .cursor = buffer,
        .end = buffer + size
    };
}

void brenda_stream_format(Brenda_Stream *s, byte *f, ...) {
    va_list args;
    va_start(args, f);
    s->cursor = format_variadic(s->cursor, s->end, f, args);
    va_end(args);
}

psh_s8 brenda_stream_end(Brenda_Stream s) {
    usize len = MIN(s.cursor - s.start, s.end - s.start);
    return s8(s.start, len);
}

void brenda_debug_draw(i32 x, i32 y, byte *fmt, ...) {
    Brenda_Stream s = brenda_stream_start_from_arena(&state.tmp, 256);

    va_list args;
    va_start(args, fmt);
    s.cursor = format_variadic(s.cursor, s.end, fmt, args);
    va_end(args);

    s8 text = brenda_stream_end(s);
    byte *cursor = text.s;
    byte *end = text.s + text.len;
    while (cursor < end) {
        TerminalTextUnit text_unit = utf8_next(&cursor, end);
        debug_text_unit_put(x, y, text_unit);
        x += text_unit.cell_width;
    }
}

void brenda_line_draw(i32 x0, i32 y0, i32 x1, i32 y1, byte *text, usize length) {
    byte *cursor = text;
    TerminalTextUnit text_unit = utf8_next(&cursor, text + length);

    if (x0 == x1) { // vertical
        if (y1 < y0) {
            i32 tmp = y0;
            y0 = y1;
            y1 = tmp;
        }

        for (i32 y = y0; y <= y1; y++) {
            text_unit_put(x0, y, text_unit);
        }
    }
    else if (y0 == y1) { // horizontal
        if (x1 < x0) {
            i32 tmp = x0;
            x0 = x1;
            x1 = tmp;
        }

        for (i32 x = x0; x <= x1; x++) {
            text_unit_put(x, y0, text_unit);
        }
    }
}

void brenda_box_draw(Brenda_Rectangle r) {
    if (r.w < 2 || r.h < 2) return;

    i32 x0 = r.x;
    i32 y0 = r.y;
    i32 x1 = r.x + r.w - 1;
    i32 y1 = r.y + r.h - 1;

    brenda_text_put(x0, y0, (byte *)"┌", sizeof("┌") - 1);
    brenda_text_put(x1, y0, (byte *)"┐", sizeof("┐") - 1);
    brenda_text_put(x0, y1, (byte *)"└", sizeof("└") - 1);
    brenda_text_put(x1, y1, (byte *)"┘", sizeof("┘") - 1);

    brenda_line_draw(x0 + 1, y0, x1 - 1, y0, (byte *)"─", sizeof("─") - 1);
    brenda_line_draw(x0 + 1, y1, x1 - 1, y1, (byte *)"─", sizeof("─") - 1);
    brenda_line_draw(x0, y0 + 1, x0, y1 - 1, (byte *)"│", sizeof("│") - 1);
    brenda_line_draw(x1, y0 + 1, x1, y1 - 1, (byte *)"│", sizeof("│") - 1);
}

void brenda_box_fill(Brenda_Rectangle r, Brenda_Effect e) {
    for (i32 j = 0; j < r.h; ++j) {
        for (i32 i = 0; i < r.w; ++i) {
            brenda_effect_put(r.x + i, r.y + j, e);
        }
    }
}

static Brenda_Stream brenda_stream_start_from_arena(Arena *arena, usize size) {
    byte *buffer = arena_push(arena, byte, size);
    assert(buffer && "arena does not have enough memory");
    return brenda_stream_start(buffer, size);
}

static void debug_text_unit_put(i32 x, i32 y, TerminalTextUnit text_unit) {
    u32 w = state.width;
    Cell *cells = state.back_buffer.items;
    cells[x + y * w] = cell(text_unit, (Brenda_Effect) {0});
}

static TerminalTextUnit text_unit_from_bytes(byte *utf8, u8 utf8_length, u8 cell_width) {
    TerminalTextUnit text_unit = {
        .utf8_length = utf8_length,
        .cell_width = cell_width,
    };
    memcpy(text_unit.utf8, utf8, utf8_length);
    return text_unit;
}

static TerminalTextUnit text_unit_from_byte(byte value) {
    return (TerminalTextUnit) {
        .utf8 = {value},
        .utf8_length = 1,
        .cell_width = 1,
    };
}

static u8 utf8_expected_length(byte first) {
    u8 value = (u8)first;
    if (value < 0x80) return 1;
    if ((value & 0xE0) == 0xC0) return 2;
    if ((value & 0xF0) == 0xE0) return 3;
    if ((value & 0xF8) == 0xF0) return 4;
    return 1;
}

static TerminalTextUnit utf8_next(byte **cursor, byte *end) {
    static TerminalTextUnit replacement = {
        .utf8 = {0xEF, 0xBF, 0xBD},
        .utf8_length = 3,
        .cell_width = 1,
    };

    byte *start = *cursor;
    if (start >= end) return replacement;

    u8 first = start[0];
    if (first < 0x80) {
        *cursor += 1;
        return text_unit_from_byte(first);
    }

    usize length = 0;
    Unicode codepoint = 0;

    if ((first & 0xE0) == 0xC0) {
        length = 2;
        codepoint = first & 0x1F;
    } else if ((first & 0xF0) == 0xE0) {
        length = 3;
        codepoint = first & 0x0F;
    } else if ((first & 0xF8) == 0xF0) {
        length = 4;
        codepoint = first & 0x07;
    } else {
        *cursor += 1;
        return replacement;
    }

    if (start + length > end) {
        *cursor = end;
        return replacement;
    }

    for (usize i = 1; i < length; ++i) {
        if (((u8)start[i] & 0xC0) != 0x80) {
            *cursor += i;
            return replacement;
        }
        codepoint = (codepoint << 6) | ((u8)start[i] & 0x3F);
    }

    *cursor += length;
    return text_unit_from_bytes(start, length, cell_width_from_unicode(codepoint));
}

static u8 cell_width_from_unicode(Unicode codepoint) {
    if (codepoint < 32 || (codepoint >= 0x7F && codepoint < 0xA0)) return 0;

    if ((codepoint >= 0x0300 && codepoint <= 0x036F) ||
        (codepoint >= 0x1AB0 && codepoint <= 0x1AFF) ||
        (codepoint >= 0x1DC0 && codepoint <= 0x1DFF) ||
        (codepoint >= 0x20D0 && codepoint <= 0x20FF) ||
        (codepoint >= 0xFE20 && codepoint <= 0xFE2F)) {
        return 0;
    }

    if ((codepoint >= 0x1100 && codepoint <= 0x115F) ||
        (codepoint >= 0x2329 && codepoint <= 0x232A) ||
        (codepoint >= 0x2E80 && codepoint <= 0xA4CF) ||
        (codepoint >= 0xAC00 && codepoint <= 0xD7A3) ||
        (codepoint >= 0xF900 && codepoint <= 0xFAFF) ||
        (codepoint >= 0xFE10 && codepoint <= 0xFE19) ||
        (codepoint >= 0xFE30 && codepoint <= 0xFE6F) ||
        (codepoint >= 0xFF00 && codepoint <= 0xFF60) ||
        (codepoint >= 0xFFE0 && codepoint <= 0xFFE6) ||
        (codepoint >= 0x1F300 && codepoint <= 0x1F64F) ||
        (codepoint >= 0x1F900 && codepoint <= 0x1F9FF)) {
        return 2;
    }

    return 1;
}
