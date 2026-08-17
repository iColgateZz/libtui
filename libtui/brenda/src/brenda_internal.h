#ifndef LIBTUI_BRENDA_INTERNAL_INCLUDE
#define LIBTUI_BRENDA_INTERNAL_INCLUDE

#include "brenda.h"

#define PSH_CORE_NO_PREFIX
#include "psh_core.h"

#include <stdarg.h>
#include <signal.h>
#include <termios.h>

enum {
    CELL_REGULAR      = 0x00,
    CELL_CONTINUATION = 0x01,
    CELL_WIDE_LEAD    = 0x02,
};

typedef u32 Unicode;

typedef struct {
    Unicode value;
    u8 is_valid;
} Utf8Codepoint;

typedef struct {
    byte utf8[4];
    u8 utf8_length;
    u8 cell_width;
} TerminalTextUnit;

typedef struct {
    Brenda_Color fg;
    Brenda_Color bg;
    u8 flags;
} Effect;

typedef struct {
    TerminalTextUnit text_unit;
    u8 flags;
    Effect effect;
} Cell;

list_def(Cell)
list_def(Brenda_Rectangle)
list_def(byte)
list_def(Brenda_Event)

typedef struct {
    i32 x, y;
    b32 is_visible;
    b32 terminal_is_visible;
    b32 position_needs_update;
} CursorState;

typedef struct {
    struct termios original_terminal;
    struct sigaction original_winch_action;
    Brenda_TerminalConfig terminal_config;
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
    CursorState cursor;
    b32 already_deinitialized;
} State;

static inline TerminalTextUnit text_unit_from_bytes(byte *utf8, u8 utf8_length, u8 cell_width);
static inline TerminalTextUnit text_unit_from_byte(byte value);
static inline u8 get_expected_utf8_length(byte first);
static inline Utf8Codepoint decode_utf8_codepoint(byte **cursor, byte *end);
static inline TerminalTextUnit decode_terminal_text_unit(byte **cursor, byte *end);
static inline u8 get_cell_width_from_unicode(Unicode codepoint);
static inline Effect get_effect_from_text_effect(Brenda_TextEffect text_effect);
static inline void effect_merge(Effect *effect, Effect new_effect);
static inline Cell cell(TerminalTextUnit text_unit, Effect effect);
static inline Cell empty_cell(void);
static inline b32 equal_cells(Cell a, Cell b);
static inline void update_screen_dimensions(void);
static inline void handle_winch_signal(i32 signal_number);
static inline i64 get_time_ms(void);
static inline i64 get_time_ns(void);
static inline void poll_events(i64 interval_ns);
static inline void handle_available_events(i32 timeout_ms);
static inline void parse_pending_input(void);
static inline void write_output(byte *text, usize length);
static inline void emit_cursor_move(List(byte) *output, u32 row, u32 column);
static inline void render_frame(void);
static inline void update_root_clip(void);
static inline b32 parse_input_unit(byte **cursor, byte *end, Brenda_Event *event);
static inline b32 parse_escape(byte **cursor, byte *end, Brenda_Event *event);
static inline b32 parse_mouse(byte **cursor, byte *end, Brenda_Event *event);
static inline b32 parse_term_key(byte **cursor, byte *end, Brenda_Event *event);
static inline b32 parse_text(byte **cursor, byte *end, Brenda_Event *event);
static inline void put_text_unit(i32 x, i32 y, TerminalTextUnit text_unit, Effect effect);
static inline void fix_wide_character(i32 x, i32 y);
static inline void emit_cells(List(byte) *output, Cell *cells, usize start, usize length);
static inline void emit_effect(List(byte) *output, Effect effect);
static inline void emit_effect_reset(List(byte) *output);
static inline byte *format_variadic(byte *cursor, byte *end, byte *format, va_list arguments);
static inline byte *format_uint(byte *cursor, byte *end, u64 value, u8 base);
static inline byte *format_cstring(byte *cursor, byte *end, byte *text);
static inline byte *format_string(byte *cursor, byte *end, s8 text);
static inline void put_debug_text_unit(i32 x, i32 y, TerminalTextUnit text_unit);
static inline Brenda_Stream arena_start_stream(Arena *arena, usize size);
static inline b32 rectangle_contains_point(Brenda_Rectangle rectangle, i32 x, i32 y);
static inline Brenda_Rectangle intersect_rectangles(Brenda_Rectangle a, Brenda_Rectangle b);
static inline b32 equal_effects(Effect a, Effect b);

#endif
