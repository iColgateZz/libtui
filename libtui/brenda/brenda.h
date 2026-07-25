#ifndef LIBTUI_BRENDA_INCLUDE
#define LIBTUI_BRENDA_INCLUDE

#include "psh_core.h"

// #define BRENDA_CTRL(value) psh_cp_from_byte((value) & 0x1F)

typedef struct {
    u8 r, g, b;
} Brenda_RGB;

enum {
    BRENDA_TEXT_EFFECT_BOLD          = 1 << 0,
    BRENDA_TEXT_EFFECT_DIM           = 1 << 1,
    BRENDA_TEXT_EFFECT_ITALIC        = 1 << 2,
    BRENDA_TEXT_EFFECT_UNDERLINE     = 1 << 3,
    BRENDA_TEXT_EFFECT_INVERSE       = 1 << 4,
    BRENDA_TEXT_EFFECT_STRIKETHROUGH = 1 << 5,
};

typedef struct {
    Brenda_RGB color;
    u8 flags;
} Brenda_TextEffect;

typedef enum {
    BRENDA_TERM_KEY_BACKSPACE = 8,
    BRENDA_TERM_KEY_TAB       = 9,
    BRENDA_TERM_KEY_ENTER     = 13,
    BRENDA_TERM_KEY_ESCAPE    = 27,

    BRENDA_TERM_KEY_INSERT   = -1,
    BRENDA_TERM_KEY_DELETE   = -2,
    BRENDA_TERM_KEY_HOME     = -3,
    BRENDA_TERM_KEY_END      = -4,
    BRENDA_TERM_KEY_PAGE_UP  = -5,
    BRENDA_TERM_KEY_PAGE_DOWN = -6,
    BRENDA_TERM_KEY_UP       = -7,
    BRENDA_TERM_KEY_DOWN     = -8,
    BRENDA_TERM_KEY_LEFT     = -9,
    BRENDA_TERM_KEY_RIGHT    = -10,
} Brenda_TermKey;

typedef enum {
    BRENDA_EVENT_NONE,
    BRENDA_EVENT_TERM_KEY,
    BRENDA_EVENT_UTF8,
    BRENDA_EVENT_WINCH,
    BRENDA_EVENT_MOUSE_LEFT,
    BRENDA_EVENT_MOUSE_RIGHT,
    BRENDA_EVENT_MOUSE_MIDDLE,
    BRENDA_EVENT_SCROLL_UP,
    BRENDA_EVENT_SCROLL_DOWN,
    BRENDA_EVENT_MOUSE_MOVE,
    BRENDA_EVENT_MOUSE_DRAG,
} Brenda_EventType;

typedef struct {
    Brenda_EventType type;
    union {
        struct {
            i32 x, y;
            b32 pressed;
        } mouse;
        struct {
            byte bytes[4];
            u8 length; // 1-4
        } utf8;
        Brenda_TermKey term_key;
    } as;
} Brenda_Event;

typedef struct {
    Brenda_Event *items;
    isize count;
} Brenda_EventSlice;

typedef struct {
    i32 x, y, w, h;
} Brenda_Rectangle;

typedef struct {
    byte *start;
    byte *cursor;
    byte *end;
} Brenda_Stream;

Brenda_EventSlice brenda_events_get(void);

void brenda_clip_push(i32 x, i32 y, i32 w, i32 h);
void brenda_clip_push_rectangle(Brenda_Rectangle rectangle);
Brenda_Rectangle brenda_clip_pop(void);
Brenda_Rectangle brenda_clip_peek(void);

void brenda_terminal_init(void);
void brenda_terminal_set_fps(i32 fps);
u32 brenda_terminal_get_width(void);
u32 brenda_terminal_get_height(void);

void brenda_frame_begin(void);
void brenda_frame_end(void);
u64 brenda_frame_get_delta_time(void);

i32 brenda_text_measure_width(byte *text, isize length);
void brenda_text_draw(i32 x, i32 y, byte *text, isize length, Brenda_TextEffect effect);
void brenda_line_draw(i32 x0, i32 y0, i32 x1, i32 y1, byte *utf8, isize byte_count, Brenda_TextEffect effect);
void brenda_box_draw(Brenda_Rectangle rectangle, Brenda_TextEffect effect);
void brenda_rectangle_fill(Brenda_Rectangle rectangle, Brenda_RGB color);

byte *brenda_format(byte *cursor, byte *end, byte *format, ...);
Brenda_Stream brenda_stream_start(byte *buffer, usize size);
void brenda_stream_format(Brenda_Stream *stream, byte *format, ...);
psh_s8 brenda_stream_end(Brenda_Stream stream);

void brenda_debug_draw(i32 x, i32 y, byte *format, ...);

#endif
