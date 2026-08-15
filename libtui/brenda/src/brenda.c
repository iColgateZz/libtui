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

u64 brenda_get_frame_delta_time(void) { return state.delta_time; }
u32 brenda_get_terminal_width(void) { return state.width; }
u32 brenda_get_terminal_height(void) { return state.height; }
Brenda_EventSlice brenda_get_events(void) {
    return (Brenda_EventSlice) {
        .items = state.events.items,
        .count = state.events.count,
    };
}

#define write_string(string) write_output(string, sizeof(string) - 1)

static void write_output(byte *text, usize length) { write(STDOUT_FILENO, text, length); }

void brenda_init_terminal(Brenda_TerminalConfig config) {
    assert(tcgetattr(STDIN_FILENO, &state.original_terminal) == 0);
    assert(sigaction(SIGWINCH, NULL, &state.original_winch_action) == 0);

    struct termios raw = state.original_terminal;
    raw.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | INPCK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    raw.c_oflag &= ~OPOST;
    raw.c_cflag &= ~(CSIZE | PARENB);
    raw.c_cflag |= CS8;
    raw.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    assert(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0);

    state.terminal_config = config;
    write_string("\33[?2004l");                 // disable bracketed paste mode
    if (config.screen_mode == BRENDA_SCREEN_ALTERNATE)
        write_string("\33[?1049h");             // use alternate buffer

    write_string("\33[?25l");                   // hide cursor

    switch (config.mouse_tracking) {
        case BRENDA_MOUSE_TRACKING_ALL_MOTION: write_string("\33[?1003h"); break;
        case BRENDA_MOUSE_TRACKING_DRAG:       write_string("\33[?1002h"); break;
        case BRENDA_MOUSE_TRACKING_CLICKS:     write_string("\33[?1000h"); break;
        case BRENDA_MOUSE_TRACKING_DISABLED: break;
    }

    if (config.mouse_tracking != BRENDA_MOUSE_TRACKING_DISABLED) 
        write_string("\33[?1006h");             // use mouse sgr protocol
    write_string("\33[0m");                     // reset text attributes
    write_string("\33[2J");                     // clear screen
    write_string("\33[H");                      // move cursor to home position

    update_screen_dimensions();
    assert(atexit(brenda_deinit_terminal) == 0);
    assert(pipe_open(&state.pipe));

    struct sigaction sa = {0};
    sa.sa_handler = handle_winch_signal;
    sa.sa_flags = SA_RESTART;
    assert(sigaction(SIGWINCH, &sa, NULL) == 0);

    list_resize(&state.back_buffer, state.width * state.height);
    list_resize(&state.front_buffer, state.width * state.height);
    list_resize(&state.frame_commands, state.width * state.height);

    // manually add the terminal scope
    Brenda_Rectangle r = {.w = state.width, .h = state.height};
    list_append(&state.clips, r);

    state.tmp = arena_init(MB(16));
}

static void update_screen_dimensions(void) {
    struct winsize ws = {0};
    assert(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0);

    state.width = ws.ws_col;
    state.height = ws.ws_row;
}

static void update_root_clip(void) {
    Brenda_Rectangle r = {.w = state.width, .h = state.height};
    state.clips.items[0] = r;
}

void brenda_deinit_terminal(void) {
    if (state.already_deinitialized) return;

    assert(sigaction(SIGWINCH, &state.original_winch_action, NULL) == 0);
    assert(tcsetattr(STDIN_FILENO, TCSAFLUSH, &state.original_terminal) == 0);

    if (state.terminal_config.mouse_tracking != BRENDA_MOUSE_TRACKING_DISABLED) {
        write_string("\33[?1000l");             // disable mouse
        write_string("\33[?1002l");             // disable mouse
        write_string("\33[?1003l");             // disable mouse
    }

    write_string("\33[0m");                     // reset text attributes
    write_string("\33[?25h");                   // show cursor
    if (state.terminal_config.screen_mode == BRENDA_SCREEN_ALTERNATE) write_string("\33[?1049l");

    fd_close(state.pipe.read_fd);
    fd_close(state.pipe.write_fd);

    list_free(state.back_buffer);
    list_free(state.front_buffer);
    list_free(state.frame_commands);
    list_free(state.clips);
    list_free(state.events);
    list_free(state.input_bytes);

    arena_destroy(state.tmp);
    state.already_deinitialized = true;
}

void brenda_show_cursor(void) { state.cursor.is_visible = true; }
void brenda_hide_cursor(void) { state.cursor.is_visible = false; }
void brenda_set_cursor_position(i32 x, i32 y) {
    state.cursor.x = x;
    state.cursor.y = y;
}

static void handle_winch_signal(i32 signo) {
    write(state.pipe.write_fd, &signo, sizeof signo);
}

void brenda_set_terminal_fps(i32 fps) {
    state.frame_interval_ns = fps <= 0 ? -1 : 1000000000ull / fps;
}

void brenda_begin_frame(void) {
    state.saved_time = get_time_ms();

    arena_clear(&state.tmp);
    list_clear(&state.frame_commands);
    list_clear(&state.events);
    for (isize i = 0; i < state.back_buffer.count; ++i)
        state.back_buffer.items[i] = empty_cell();

    poll_events(state.frame_interval_ns);
}

static i64 get_time_ns(void) {
    struct timespec ts = {0};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

static i64 get_time_ms(void) {
    return get_time_ns() / 1000000ull;
}

void brenda_end_frame(void) {
    render_frame();

    s8 cursor_visibility;
    if (state.cursor.is_visible) {
        emit_cursor_move(&state.frame_commands, state.cursor.y, state.cursor.x);
        cursor_visibility = s8("\33[?25h");
    } else {
        cursor_visibility = s8("\33[?25l");
    }
    list_append_many(&state.frame_commands, cursor_visibility.s, cursor_visibility.len);

    write_output(state.frame_commands.items, state.frame_commands.count);
    state.delta_time = get_time_ms() - state.saved_time;
}

//TODO: maybe hash each row and compare hashes?
static void render_frame(void) {
    Cell *back_items = state.back_buffer.items;
    Cell *front_items = state.front_buffer.items;
    u32 screen_w = state.width;

    for (u32 row = 0; row < state.height; row++) {
        usize row_start = row * screen_w;
        usize row_end   = row_start + screen_w;

        usize pos = row_start;
        while (pos < row_end) {
            if (equal_cells(back_items[pos], front_items[pos])) {
                pos++;
                continue;
            }

            usize run_start = pos;
            Effect run_effect = back_items[run_start].effect;
            while (pos < row_end && 
                !equal_cells(back_items[pos], front_items[pos]) &&
                equal_effects(back_items[pos].effect, run_effect)) {
                pos++;
            }

            usize run_len = pos - run_start;
            u32 new_row = run_start / screen_w;
            u32 new_col = run_start % screen_w;
            emit_cursor_move(&state.frame_commands, new_row, new_col);
            emit_cells(&state.frame_commands, back_items, run_start, run_len);

            memcpy(
                front_items + run_start,
                back_items + run_start,
                run_len * sizeof(Cell)
            );
        }
    }
}

static void emit_cells(List(byte) *out, Cell *cells, usize start, usize len) {
    emit_effect(out, cells[start].effect);

    for (usize i = 0; i < len; i++) {
        Cell c = cells[start + i];
        if (c.flags & CELL_CONTINUATION) continue;

        list_append_many(out, c.text_unit.utf8, c.text_unit.utf8_length);
    }

    emit_effect_reset(out);
}

static void emit_effect(List(byte) *out, Effect e) {
    if (e.flags == 0 && !e.fg.is_set && !e.bg.is_set) return;

    Brenda_Stream s = arena_start_stream(&state.tmp, 64);
    brenda_stream_format(&s, "\33[");

    if (e.flags & BRENDA_TEXT_EFFECT_BOLD)          brenda_stream_format(&s, "1;");
    if (e.flags & BRENDA_TEXT_EFFECT_DIM)           brenda_stream_format(&s, "2;");
    if (e.flags & BRENDA_TEXT_EFFECT_ITALIC)        brenda_stream_format(&s, "3;");
    if (e.flags & BRENDA_TEXT_EFFECT_UNDERLINE)     brenda_stream_format(&s, "4;");
    if (e.flags & BRENDA_TEXT_EFFECT_INVERSE)       brenda_stream_format(&s, "7;");
    if (e.flags & BRENDA_TEXT_EFFECT_STRIKETHROUGH) brenda_stream_format(&s, "9;");

    if (e.fg.is_set)
        brenda_stream_format(&s, "38;2;%u;%u;%u;", e.fg.r, e.fg.g, e.fg.b);
    if (e.bg.is_set)
        brenda_stream_format(&s, "48;2;%u;%u;%u;", e.bg.r, e.bg.g, e.bg.b);

    // replace ';' with 'm'
    *(s.cursor - 1) = 'm';
    s8 result = brenda_stream_end(s);
    list_append_many(out, result.s, result.len);
}

static void emit_effect_reset(List(byte) *out) {
    s8 result = s8("\33[0m");
    list_append_many(out, result.s, result.len);
}

static void emit_cursor_move(List(byte) *a, u32 row, u32 col) {
    Brenda_Stream s = arena_start_stream(&state.tmp, 64);
    brenda_stream_format(&s, "\33[%u;%uH", row + 1, col + 1);
    s8 result = brenda_stream_end(s);

    list_append_many(a, result.s, result.len);
}

static void poll_events(i64 interval_ns) {
    if (interval_ns <= 0) {
        handle_available_events(-1);
        if (state.input_bytes.count == 1 && state.input_bytes.items[0] == BRENDA_TERM_KEY_ESCAPE)
            handle_available_events(5);
    } else {
        i64 deadline_ns = get_time_ns() + interval_ns;
        for (;;) {
            i64 remaining_ns = deadline_ns - get_time_ns();
            if (remaining_ns <= 0) break;
            i32 timeout_ms = (remaining_ns + 999999) / 1000000;
            handle_available_events(timeout_ms);
        }
    }

    if (state.input_bytes.count == 1 && state.input_bytes.items[0] == BRENDA_TERM_KEY_ESCAPE) {
        list_append(&state.events, ((Brenda_Event) {
            .type = BRENDA_EVENT_TERM_KEY,
            .as.term_key = BRENDA_TERM_KEY_ESCAPE,
        }));
        list_clear(&state.input_bytes);
    }
}

static void handle_available_events(i32 timeout_ms) {
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

        update_screen_dimensions();
        update_root_clip();

        u32 new_size = state.width * state.height;
        list_resize(&state.back_buffer, new_size);
        list_resize(&state.front_buffer, new_size);

        // trigger full redraw
        for (isize i = 0; i < state.front_buffer.count; ++i)
            state.front_buffer.items[i] = cell(text_unit_from_byte(0xFF), (Effect) {0});
    }

    if (pfd[1].revents & POLLIN) {
        //TODO: Maybe read directly into input_bytes?
        static byte buffer[4096];
        isize n = read(STDIN_FILENO, buffer, sizeof buffer);

        assert(n > 0 && "read non-positive amount of bytes from STDIN");

        list_append_many(&state.input_bytes, buffer, n);
        parse_pending_input();
    }
}

//TODO: support extended keyboard protocol
//TODO: add syncronized output
//TODO: support OSC 8 hyperlinks
static void parse_pending_input(void) {
    byte *start = state.input_bytes.items;
    byte *p = start;
    byte *end = start + state.input_bytes.count;

    while (p < end) {
        byte *before = p;
        Brenda_Event e = { .type = BRENDA_EVENT_NONE };

        if (*p != BRENDA_TERM_KEY_ESCAPE) {
            if (!parse_input_unit(&p, end, &e)) break;
        } else if (end - p <= 1) {
            break;
        } else if (p[1] == '[' || p[1] == 'O') {
            if (!parse_escape(&p, end, &e)) break;
        } else {
            byte *alt_input = p + 1;
            if (!parse_input_unit(&alt_input, end, &e)) break;
            e.modifiers |= BRENDA_MODIFIER_ALT;
            p = alt_input;
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

// UTF-8 text or a single-byte terminal control key.
static b32 parse_input_unit(byte **p, byte *end, Brenda_Event *e) {
    u8 value = (u8)**p;

    if (value == 127) {
        (*p)++;
        e->type = BRENDA_EVENT_TERM_KEY;
        e->as.term_key = BRENDA_TERM_KEY_BACKSPACE;
        return true;
    }

    if (value == '\t' || value == '\r' || value == '\n') {
        (*p)++;
        e->type = BRENDA_EVENT_TERM_KEY;
        e->as.term_key = value == '\t' ? BRENDA_TERM_KEY_TAB : BRENDA_TERM_KEY_ENTER;
        return true;
    }

    if (value <= 31 && value != BRENDA_TERM_KEY_ESCAPE) {
        (*p)++;
        e->type = BRENDA_EVENT_UTF8;
        e->modifiers = BRENDA_MODIFIER_CTRL;
        e->as.utf8.bytes[0] = value == 0 ? ' ' : "@abcdefghijklmnopqrstuvwxyz[\\]^_"[value];
        e->as.utf8.length = 1;
        return true;
    }

    return parse_text(p, end, e);
}

// CSI (ESC [) or SS3 (ESC O) terminal sequence.
static b32 parse_escape(byte **p, byte *end, Brenda_Event *e) {
    byte *start = *p;
    assert(end - start >= 2);
    assert(start[1] == '[' || start[1] == 'O');

    if (parse_mouse(p, end, e)) return true;
    if (parse_term_key(p, end, e)) return true;

    // Getting rid of unknown sequence
    for (byte *it = start + 2; it < end; ++it) {
        if (0x40 <= *it && *it <= 0x7E) {
            *p = it + 1;
            return true;
        }
    }

    return false;
}

// SGR mouse sequence: CSI < button ; x ; y M/m.
static b32 parse_mouse(byte **p, byte *end, Brenda_Event *e) {
    byte *start = *p;
    isize n = end - start;
    if (n < 9 || memcmp(start, "\33[<", 3) != 0) return false;

    byte *terminator_m = memchr(start, 'm', n);
    byte *terminator_M = memchr(start, 'M', n);
    byte *terminator = terminator_m;
    if (terminator == NULL || (terminator_M != NULL && terminator_M < terminator)) terminator = terminator_M;
    if (!terminator) return false;

    byte *cursor        = start;
    u32 btn             = strtol(cursor + 3, &cursor, 10);
    e->as.mouse.x       = strtol(cursor + 1, &cursor, 10) - 1;
    e->as.mouse.y       = strtol(cursor + 1, &cursor, 10) - 1;
    e->as.mouse.pressed = (*cursor == 'M');

    if (btn & 4)  e->modifiers |= BRENDA_MODIFIER_SHIFT;
    if (btn & 8)  e->modifiers |= BRENDA_MODIFIER_ALT;
    if (btn & 16) e->modifiers |= BRENDA_MODIFIER_CTRL;

    switch (btn & ~(4 | 8 | 16)) {
        case 0:  e->type = BRENDA_EVENT_MOUSE_LEFT;   break;
        case 1:  e->type = BRENDA_EVENT_MOUSE_MIDDLE; break;
        case 2:  e->type = BRENDA_EVENT_MOUSE_RIGHT;  break;
        case 32:
        case 33:
        case 34: e->type = BRENDA_EVENT_MOUSE_DRAG;   break;
        case 35: e->type = BRENDA_EVENT_MOUSE_MOVE;   break;
        case 64: e->type = BRENDA_EVENT_SCROLL_UP;    break;
        case 65: e->type = BRENDA_EVENT_SCROLL_DOWN;  break;
        default: assert(false && "Unknown mouse event");
    }

    *p = terminator + 1;
    return true;
}

// CSI or SS3 navigation, editing and function key.
static b32 parse_term_key(byte **p, byte *end, Brenda_Event *e) {
    byte *start = *p;
    isize n = end - start;
    if (n < 3) return false;

    if (start[1] == 'O') {
        switch (start[2]) {
            case 'A': e->as.term_key = BRENDA_TERM_KEY_UP;    break;
            case 'B': e->as.term_key = BRENDA_TERM_KEY_DOWN;  break;
            case 'C': e->as.term_key = BRENDA_TERM_KEY_RIGHT; break;
            case 'D': e->as.term_key = BRENDA_TERM_KEY_LEFT;  break;
            case 'H': e->as.term_key = BRENDA_TERM_KEY_HOME;  break;
            case 'F': e->as.term_key = BRENDA_TERM_KEY_END;   break;
            case 'P': e->as.term_key = BRENDA_TERM_KEY_F1;    break;
            case 'Q': e->as.term_key = BRENDA_TERM_KEY_F2;    break;
            case 'R': e->as.term_key = BRENDA_TERM_KEY_F3;    break;
            case 'S': e->as.term_key = BRENDA_TERM_KEY_F4;    break;
            default: return false;
        }

        e->type = BRENDA_EVENT_TERM_KEY;
        *p = start + 3;
        return true;
    }

    byte *final = start + 2;
    while (final < end && !('@' <= *final && *final <= '~')) final++;
    if (final == end) return false;

    u32 parameters[2] = {0};
    usize parameter_count = 0;
    byte *cursor = start + 2;
    while (cursor < final) {
        if (parameter_count == ARRAY_SIZE(parameters) || *cursor < '0' || *cursor > '9') return false;

        u32 value = 0;
        while (cursor < final && '0' <= *cursor && *cursor <= '9') {
            value = value * 10 + (*cursor - '0');
            cursor++;
        }
        parameters[parameter_count++] = value;

        if (cursor == final) break;
        if (*cursor != ';') return false;
        cursor++;
    }

    u32 modifier_parameter = parameter_count == 2 ? parameters[1] : 1;
    if (modifier_parameter == 0) return false;
    u32 modifier_bits = modifier_parameter - 1;
    if (modifier_bits & 1) e->modifiers |= BRENDA_MODIFIER_SHIFT;
    if (modifier_bits & 2) e->modifiers |= BRENDA_MODIFIER_ALT;
    if (modifier_bits & 4) e->modifiers |= BRENDA_MODIFIER_CTRL;

    switch (*final) {
        case 'A': e->as.term_key = BRENDA_TERM_KEY_UP;    break;
        case 'B': e->as.term_key = BRENDA_TERM_KEY_DOWN;  break;
        case 'C': e->as.term_key = BRENDA_TERM_KEY_RIGHT; break;
        case 'D': e->as.term_key = BRENDA_TERM_KEY_LEFT;  break;
        case 'H': e->as.term_key = BRENDA_TERM_KEY_HOME;  break;
        case 'F': e->as.term_key = BRENDA_TERM_KEY_END;   break;
        case 'P': e->as.term_key = BRENDA_TERM_KEY_F1;    break;
        case 'Q': e->as.term_key = BRENDA_TERM_KEY_F2;    break;
        case 'R': e->as.term_key = BRENDA_TERM_KEY_F3;    break;
        case 'S': e->as.term_key = BRENDA_TERM_KEY_F4;    break;
        case 'Z':
            e->as.term_key = BRENDA_TERM_KEY_TAB;
            e->modifiers |= BRENDA_MODIFIER_SHIFT;
            break;
        case '~':
            if (parameter_count == 0) return false;
            switch (parameters[0]) {
                case 1:
                case 7:  e->as.term_key = BRENDA_TERM_KEY_HOME;      break;
                case 2:  e->as.term_key = BRENDA_TERM_KEY_INSERT;    break;
                case 3:  e->as.term_key = BRENDA_TERM_KEY_DELETE;    break;
                case 4:
                case 8:  e->as.term_key = BRENDA_TERM_KEY_END;       break;
                case 5:  e->as.term_key = BRENDA_TERM_KEY_PAGE_UP;   break;
                case 6:  e->as.term_key = BRENDA_TERM_KEY_PAGE_DOWN; break;
                case 15: e->as.term_key = BRENDA_TERM_KEY_F5;        break;
                case 17: e->as.term_key = BRENDA_TERM_KEY_F6;        break;
                case 18: e->as.term_key = BRENDA_TERM_KEY_F7;        break;
                case 19: e->as.term_key = BRENDA_TERM_KEY_F8;        break;
                case 20: e->as.term_key = BRENDA_TERM_KEY_F9;        break;
                case 21: e->as.term_key = BRENDA_TERM_KEY_F10;       break;
                case 23: e->as.term_key = BRENDA_TERM_KEY_F11;       break;
                case 24: e->as.term_key = BRENDA_TERM_KEY_F12;       break;
                default: return false;
            }
            break;
        default: return false;
    }

    e->type = BRENDA_EVENT_TERM_KEY;
    *p = final + 1;
    return true;
}

// One UTF-8 encoded text unit.
static b32 parse_text(byte **p, byte *end, Brenda_Event *e) {
    byte *start = *p;
    u8 expected_length = get_expected_utf8_length(*start);
    if (end - start < expected_length) return false;

    //TODO: parse_next_utf8_unit also decodes width. It is not needed here.
    TerminalTextUnit text_unit = parse_next_utf8_unit(p, start + expected_length);
    e->type = BRENDA_EVENT_UTF8;
    e->as.utf8.length = text_unit.utf8_length;
    memcpy(e->as.utf8.bytes, text_unit.utf8, text_unit.utf8_length);
    return true;
}

static inline Effect get_effect_from_text_effect(Brenda_TextEffect text_effect) {
    return (Effect) {
        .fg = text_effect.color,
        .flags = text_effect.flags,
    };
}

static void effect_merge(Effect *effect, Effect new_effect) {
    effect->flags |= new_effect.flags;
    if (new_effect.fg.is_set) effect->fg = new_effect.fg;
    if (new_effect.bg.is_set) effect->bg = new_effect.bg;
}

static Cell cell(TerminalTextUnit text_unit, Effect effect) {
    return (Cell) { .text_unit = text_unit, .effect = effect };
}

static Cell empty_cell(void) { return (Cell) { .text_unit = text_unit_from_byte(' ') }; }
static b32 equal_cells(Cell a, Cell b) { return memcmp(&a, &b, sizeof a) == 0; }

static inline b32 equal_effects(Effect a, Effect b) { return memcmp(&a, &b, sizeof a) == 0; }

static void put_text_unit(i32 x, i32 y, TerminalTextUnit text_unit, Effect effect) {
    Brenda_Rectangle parent = brenda_peek_clip();
    if (!rectangle_contains_point(parent, x, y)) return;

    u32 w = state.width;
    Cell *cells = state.back_buffer.items;

    if (text_unit.cell_width == 1) {
        fix_wide_character(x, y);
        Cell *current = &cells[x + y * w];
        current->text_unit = text_unit;
        current->flags = CELL_REGULAR;
        effect_merge(&current->effect, effect);
        return;
    }

    if (text_unit.cell_width == 2) {
        if ((u32)x + 1 >= w) return; // cannot fit

        fix_wide_character(x, y);
        fix_wide_character(x + 1, y);

        Cell *lead = &cells[x + y * w];
        lead->text_unit = text_unit;
        lead->flags = CELL_WIDE_LEAD;
        effect_merge(&lead->effect, effect);

        Cell *cont = &cells[(x + 1) + y * w];
        cont->flags = CELL_CONTINUATION;
        effect_merge(&cont->effect, effect);
        return;
    }

    assert(false && "a terminal text unit has an invalid cell width");
}

static void fix_wide_character(i32 x, i32 y) {
    u32 w = state.width;
    Cell *cells = state.back_buffer.items;
    Cell c = cells[x + y * w];

    if ((c.flags & CELL_WIDE_LEAD) && (u32)x + 1 < w) {
        Cell *continuation = &cells[(x + 1) + y * w];
        continuation->text_unit = text_unit_from_byte(' ');
        continuation->flags = CELL_REGULAR;
    }

    if ((c.flags & CELL_CONTINUATION) && (u32)x > 0) {
        Cell *lead = &cells[(x - 1) + y * w];
        lead->text_unit = text_unit_from_byte(' ');
        lead->flags = CELL_REGULAR;
    }
}

i32 brenda_measure_text_width(byte *text, isize length) {
    i32 width = 0;
    byte *cursor = text;
    byte *end = text + length;
    while (cursor < end) width += parse_next_utf8_unit(&cursor, end).cell_width;
    return width;
}

void brenda_draw_text(i32 x, i32 y, byte *text, isize length, Brenda_TextEffect text_effect) {
    Effect effect = get_effect_from_text_effect(text_effect);
    byte *cursor = text;
    byte *end = text + length;
    while (cursor < end) {
        TerminalTextUnit text_unit = parse_next_utf8_unit(&cursor, end);
        put_text_unit(x, y, text_unit, effect);
        x += text_unit.cell_width;
    }
}

void brenda_push_clip(i32 x, i32 y, i32 w, i32 h) {
    Brenda_Rectangle r = {x,y,w,h};
    brenda_push_clip_rectangle(r);
}

void brenda_push_clip_rectangle(Brenda_Rectangle r) {
    Brenda_Rectangle parent = brenda_peek_clip();
    Brenda_Rectangle clipped = intersect_rectangles(parent, r);
    list_append(&state.clips, clipped);
}

Brenda_Rectangle brenda_pop_clip(void) {
    return list_pop(&state.clips);
}

Brenda_Rectangle brenda_peek_clip(void) {
    return list_last(&state.clips);
}

static inline b32 rectangle_contains_point(Brenda_Rectangle r, i32 x, i32 y) {
    return r.x <= x && x < r.x + r.w 
        && r.y <= y && y < r.y + r.h;
}

static inline Brenda_Rectangle intersect_rectangles(Brenda_Rectangle a, Brenda_Rectangle b) {
    i32 x1 = MAX(a.x, b.x);
    i32 y1 = MAX(a.y, b.y);
    i32 x2 = MIN(a.x + a.w, b.x + b.w);
    i32 y2 = MIN(a.y + a.h, b.y + b.h);

    if (x2 <= x1 || y2 <= y1) {
        return (Brenda_Rectangle){0,0,0,0}; // fully clipped
    }

    return (Brenda_Rectangle){ x1, y1, x2 - x1, y2 - y1 };
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

void brenda_draw_debug_text(i32 x, i32 y, byte *fmt, ...) {
    Brenda_Stream s = arena_start_stream(&state.tmp, 256);

    va_list args;
    va_start(args, fmt);
    s.cursor = format_variadic(s.cursor, s.end, fmt, args);
    va_end(args);

    s8 text = brenda_stream_end(s);
    byte *cursor = text.s;
    byte *end = text.s + text.len;
    while (cursor < end) {
        TerminalTextUnit text_unit = parse_next_utf8_unit(&cursor, end);
        put_debug_text_unit(x, y, text_unit);
        x += text_unit.cell_width;
    }
}

void brenda_draw_line(i32 x0, i32 y0, i32 x1, i32 y1, byte *text, isize length, Brenda_TextEffect text_effect) {
    byte *cursor = text;
    TerminalTextUnit text_unit = parse_next_utf8_unit(&cursor, text + length);
    Effect effect = get_effect_from_text_effect(text_effect);

    if (x0 == x1) { // vertical
        if (y1 < y0) {
            i32 tmp = y0;
            y0 = y1;
            y1 = tmp;
        }

        for (i32 y = y0; y <= y1; y++) {
            put_text_unit(x0, y, text_unit, effect);
        }
    }
    else if (y0 == y1) { // horizontal
        if (x1 < x0) {
            i32 tmp = x0;
            x0 = x1;
            x1 = tmp;
        }

        for (i32 x = x0; x <= x1; x++) {
            put_text_unit(x, y0, text_unit, effect);
        }
    }
}

void brenda_draw_box(Brenda_Rectangle r, Brenda_TextEffect effect) {
    if (r.w < 2 || r.h < 2) return;

    i32 x0 = r.x;
    i32 y0 = r.y;
    i32 x1 = r.x + r.w - 1;
    i32 y1 = r.y + r.h - 1;

    brenda_draw_text(x0, y0, (byte *)"┌", sizeof("┌") - 1, effect);
    brenda_draw_text(x1, y0, (byte *)"┐", sizeof("┐") - 1, effect);
    brenda_draw_text(x0, y1, (byte *)"└", sizeof("└") - 1, effect);
    brenda_draw_text(x1, y1, (byte *)"┘", sizeof("┘") - 1, effect);

    brenda_draw_line(x0 + 1, y0, x1 - 1, y0, (byte *)"─", sizeof("─") - 1, effect);
    brenda_draw_line(x0 + 1, y1, x1 - 1, y1, (byte *)"─", sizeof("─") - 1, effect);
    brenda_draw_line(x0, y0 + 1, x0, y1 - 1, (byte *)"│", sizeof("│") - 1, effect);
    brenda_draw_line(x1, y0 + 1, x1, y1 - 1, (byte *)"│", sizeof("│") - 1, effect);
}

void brenda_fill_rectangle(Brenda_Rectangle r, Brenda_Color color) {
    Effect effect = {.bg = color};
    Brenda_Rectangle clip = brenda_peek_clip();
    Brenda_Rectangle rectangle = intersect_rectangles(r, clip);
    Cell *cells = state.back_buffer.items;

    for (i32 y = rectangle.y; y < rectangle.y + rectangle.h; ++y) {
        for (i32 x = rectangle.x; x < rectangle.x + rectangle.w; ++x) {
            Cell *current = &cells[x + y * state.width];
            effect_merge(&current->effect, effect);

            if ((current->flags & CELL_WIDE_LEAD) && (u32)x + 1 < state.width) {
                effect_merge(&cells[x + 1 + y * state.width].effect, effect);
            } else if ((current->flags & CELL_CONTINUATION) && x > 0) {
                effect_merge(&cells[x - 1 + y * state.width].effect, effect);
            }
        }
    }
}

static Brenda_Stream arena_start_stream(Arena *arena, usize size) {
    byte *buffer = arena_push(arena, byte, size);
    assert(buffer && "arena does not have enough memory");
    return brenda_stream_start(buffer, size);
}

static void put_debug_text_unit(i32 x, i32 y, TerminalTextUnit text_unit) {
    u32 w = state.width;
    Cell *cells = state.back_buffer.items;
    cells[x + y * w] = cell(text_unit, (Effect) {0});
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

static u8 get_expected_utf8_length(byte first) {
    u8 value = (u8)first;
    if (value < 0x80) return 1;
    if ((value & 0xE0) == 0xC0) return 2;
    if ((value & 0xF0) == 0xE0) return 3;
    if ((value & 0xF8) == 0xF0) return 4;
    return 1;
}

static TerminalTextUnit parse_next_utf8_unit(byte **cursor, byte *end) {
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
    return text_unit_from_bytes(start, length, get_cell_width_from_unicode(codepoint));
}

static u8 get_cell_width_from_unicode(Unicode codepoint) {
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
