#ifndef LIBTUI_LAYLA_INCLUDE
#define LIBTUI_LAYLA_INCLUDE

#define PSH_CORE_NO_PREFIX
#include "psh_core.h"

//TODO: move to psh_core
#define typeof __typeof__
#define match(tu) for (typeof(tu) _tu = tu, *p = &_tu; p; p = NULL) switch ((tu).type)
// may be fixed with __VA_OPT(__VA_ARGS__,) in C23 or
// https://medium.com/@pauljlucas/using-advanced-c-preprocessor-macros-for-a-pre-c23-c-20-va-opt-substitute-bccefde27817
#define case(...)   xcase(__VA_ARGS__, _case2, _case1)(__VA_ARGS__)
#define xcase(a, b, c, ...) c
#define _case2(tag, varname) case (tag): ;typeof(_tu._##tag) varname = _tu._##tag;
#define _case1(tag) case (tag): ;
#define otherwise default:
#define unwrap_into(tu, tag, varname) typeof((tu)._##tag) varname = (tu)._##tag;
#define matches(tu, tag) (tu).type == tag

#define tag(T, tag, ...) ((T) {.type = tag, ._##tag = __VA_ARGS__})
#define tag0(T, tag) ((T) {.type = tag})

#define packed_enum enum __attribute__((__packed__))

//TODO: namespace all public symbols
//TODO: percentage sizing?
typedef struct {
    packed_enum {
        SIZE_FIT,
        SIZE_FILL,
        SIZE_FIXED,
    } type;
    union {
        struct {
            i32 value;
        } _SIZE_FIXED;
        struct {
            i32 min;
            i32 max;
        } _SIZE_FIT;
        struct {
            i32 min;
            i32 max;
        } _SIZE_FILL;
    };
} Layout_SizeStyle;

#define FIXED(value)    tag(Layout_SizeStyle, SIZE_FIXED, {value})
#define FIT(min, max)   tag(Layout_SizeStyle, SIZE_FIT, {min, max})
#define FILL(min, max)  tag(Layout_SizeStyle, SIZE_FILL, {min, max})

typedef struct {
    Layout_SizeStyle w;
    Layout_SizeStyle h;
} Layout_Sizing;

typedef struct {
    u8 r, g, b;
} Layout_Color;

typedef struct {
    i32 x, y, w, h;
} Layout_Rect;

typedef struct {
    packed_enum {
        DIR_ROW,
        DIR_COL,
    } type;
} Layout_Direction;

typedef struct {
    packed_enum {
        ALIGN_START,
        ALIGN_CENTER,
        ALIGN_END,
    } type;
} Layout_Alignment;

typedef enum {
    SCROLL_NONE,
    SCROLL_Y,
} Layout_Scroll;

typedef struct {
    Layout_Sizing size;
    Layout_Color color;
    //TODO: add per-size padding
    u8 padding;
    u8 spacing;
    // TODO: u8 margin;
    // BorderStyle border;
    Layout_Direction direction;
    Layout_Alignment align_children;
    Layout_Alignment align_self;
    Layout_Scroll scroll;
} Layout_ContainerStyle;

typedef struct {
    Layout_ContainerStyle style;
    // void *userdata;
} Layout_ContainerConfig;

typedef struct {
    Layout_Color color;
} Layout_TextStyle;

typedef struct {
    byte *items;
    isize count;
} Layout_TextSpan;

//TODO: text measurement results should probably be cached.
//TODO: create one internal line-breaking pass reused for height calculation and command emission
//TODO: maybe add different text wrapping policies?
//TODO: maybe add text alignment? can this be done by placing it in a container?
typedef struct {
    Layout_TextSpan text;
    Layout_TextStyle style;
} Layout_TextConfig;

#define LAYOUT_TEXT(s) ((Layout_TextSpan) {.items = (byte *)(s), .count = sizeof(s) - 1})

typedef struct {
    packed_enum {
        LAYOUT_CMD_RECT,
        LAYOUT_CMD_TEXT,
        LAYOUT_CMD_CLIP_START,
        LAYOUT_CMD_CLIP_END,
        // LAYOUT_CMD_BORDER, // pass concrete codepoints to draw
    } type;
    union {
        struct {
            i32 x, y, w, h;
            Layout_Color color;
        } _LAYOUT_CMD_RECT;
        struct {
            i32 x, y;
            Layout_TextSpan text;
            Layout_TextStyle style;
        } _LAYOUT_CMD_TEXT;
        struct {
            i32 x, y, w, h;
        } _LAYOUT_CMD_CLIP_START;
    };
} Layout_Command;

slice_def(Layout_Command);
typedef i32 Layout_PersistentID;

void layout_screen_set_dimensions(i32 w, i32 h);
void layout_cursor_set_position(i32 x, i32 y);
void layout_scroll_update(i32 delta_y);
b32 layout_cursor_is_hovered();
Layout_PersistentID layout_cursor_get_hovered_id();
void layout_begin();
Slice(Layout_Command) layout_end();
void layout_text_open(Layout_PersistentID id, Layout_TextConfig conf);
void layout_container_open(Layout_PersistentID id, Layout_ContainerConfig conf);
void layout_close();

#define Container(id, ...)                              \
    for (u8 _latch = (layout_container_open(            \
            id,                                         \
            (Layout_ContainerConfig) {                  \
                .style.size.w = FIT(0, INT32_MAX),      \
                .style.size.h = FIT(0, INT32_MAX),      \
                __VA_ARGS__                             \
        }), 0); _latch < 1; _latch = 1, layout_close())

#define Text(id, ...)              \
    for (u8 _latch = (layout_text_open(id, (Layout_TextConfig) {__VA_ARGS__}), 0); _latch < 1; _latch = 1, layout_close())

#endif
