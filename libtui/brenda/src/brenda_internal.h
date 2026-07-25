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

typedef struct {
    CodePoint codepoint;
    u8 flags;
    Brenda_Effect effect;
} Cell;

typedef struct {
    byte *start;
    byte *cursor;
    byte *end;
} Stream;

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

static Cell cell(CodePoint codepoint, Brenda_Effect effect);
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
static void wide_character_fix(i32 x, i32 y);
static void cells_emit(List(byte) *output, Cell *cells, usize start, usize length);
static void effect_emit(List(byte) *output, Brenda_Effect effect);
static void effect_reset(List(byte) *output);
static byte *format_variadic(byte *cursor, byte *end, byte *format, va_list arguments);
static byte *format_uint(byte *cursor, byte *end, u64 value, u8 base);
static byte *format_cstring(byte *cursor, byte *end, byte *text);
static byte *format_string(byte *cursor, byte *end, s8 text);
static Stream stream_start(byte *buffer, usize size);
static void stream_format(Stream *stream, byte *format, ...);
static s8 stream_end(Stream stream);
static Stream arena_stream_start(Arena *arena, usize size);

#endif
