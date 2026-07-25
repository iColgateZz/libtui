#ifndef LIBTUI_BRENDA_INTERNAL_INCLUDE
#define LIBTUI_BRENDA_INTERNAL_INCLUDE

#include "brenda.h"

#define PSH_CORE_NO_PREFIX
#include "psh_core.h"

#include <stdarg.h>
#include <termios.h>

enum {
    CELL_REGULAR      = 0x00,
    CELL_CONTINUATION = 0x01,
    CELL_WIDE_LEAD    = 0x02,
};

typedef u32 Unicode;

typedef struct {
    byte utf8[4];
    u8 utf8_length;
    u8 cell_width;
} TerminalTextUnit;

typedef struct {
    TerminalTextUnit text_unit;
    u8 flags;
    Brenda_Effect effect;
} Cell;

list_def(Cell)
list_def(Brenda_Rectangle)
list_def(byte)
list_def(Brenda_Event)

typedef struct {
    struct termios original_terminal;
    Unix_Pipe pipe;
    List(Brenda_Event) events;
    List(byte) input_bytes;
    i64 frame_interval_ns;
    u64 saved_time;
    u64 delta_time;
    List(Cell) front_buffer;
    List(Cell) back_buffer;
    List(byte) frame_commands;
    Arena tmp;
    List(Brenda_Rectangle) clips;
    u32 width;
    u32 height;
} State;

static TerminalTextUnit text_unit_from_bytes(byte *utf8, u8 utf8_length, u8 cell_width);
static TerminalTextUnit text_unit_from_byte(byte value);
static u8 utf8_expected_length(byte first);
static TerminalTextUnit utf8_next(byte **cursor, byte *end);
static u8 cell_width_from_unicode(Unicode codepoint);
static Cell cell(TerminalTextUnit text_unit, Brenda_Effect effect);
static Cell cell_empty(void);
static b32 cell_equal(Cell a, Cell b);
static void terminal_restore(void);
static void screen_dimensions_update(void);
static void signal_winch_handle(i32 signal_number);
static i64 time_get_ms(void);
static i64 time_get_ns(void);
static void timestamp_save(void);
static void delta_time_calculate(void);
static void events_poll_until(i64 deadline_ns);
static void events_handle_available(i32 timeout_ms);
static void input_parse_pending(void);
static void output_write(byte *text, usize length);
static void cursor_move_emit(List(byte) *output, u32 row, u32 column);
static void frame_render(void);
static void root_clip_update(void);
static b32 escape_parse(byte **cursor, byte *end, Brenda_Event *event);
static b32 mouse_parse(byte **cursor, byte *end, Brenda_Event *event);
static b32 term_key_parse(byte **cursor, byte *end, Brenda_Event *event);
static b32 text_parse(byte **cursor, byte *end, Brenda_Event *event);
static void text_unit_put(i32 x, i32 y, TerminalTextUnit text_unit);
static void wide_character_fix(i32 x, i32 y);
static void cells_emit(List(byte) *output, Cell *cells, usize start, usize length);
static void effect_emit(List(byte) *output, Brenda_Effect effect);
static void effect_reset(List(byte) *output);
static byte *format_variadic(byte *cursor, byte *end, byte *format, va_list arguments);
static byte *format_uint(byte *cursor, byte *end, u64 value, u8 base);
static byte *format_cstring(byte *cursor, byte *end, byte *text);
static byte *format_string(byte *cursor, byte *end, s8 text);
static void debug_text_unit_put(i32 x, i32 y, TerminalTextUnit text_unit);
static Brenda_Stream brenda_stream_start_from_arena(Arena *arena, usize size);

#endif
