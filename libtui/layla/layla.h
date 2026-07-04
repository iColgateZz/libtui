#ifndef LIBTUI_LAYLA_INCLUDE
#define LIBTUI_LAYLA_INCLUDE

#include "psh_core.h"

#define LAYOUT_PACKED_ENUM enum __attribute__((__packed__))

//TODO: namespace all public symbols
//TODO: percentage sizing?
typedef struct {
    LAYOUT_PACKED_ENUM {
        SIZE_FIT,
        SIZE_FILL,
        SIZE_FIXED,
    } type;
    union {
        struct {
            i32 value;
        } fixed;
        struct {
            i32 min;
            i32 max;
        } fit;
        struct {
            i32 min;
            i32 max;
        } fill;
    } as;
} Layout_SizeStyle;

#define FIXED(fixed_value) ((Layout_SizeStyle) {                       \
    .type = SIZE_FIXED,                                                \
    .as.fixed = {.value = (fixed_value)},                              \
})

#define FIT(min_value, max_value) ((Layout_SizeStyle) {                \
    .type = SIZE_FIT,                                                  \
    .as.fit = {.min = (min_value), .max = (max_value)},                \
})

#define FILL(min_value, max_value) ((Layout_SizeStyle) {               \
    .type = SIZE_FILL,                                                 \
    .as.fill = {.min = (min_value), .max = (max_value)},               \
})

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

typedef LAYOUT_PACKED_ENUM {
    DIR_ROW,
    DIR_COL,
} Layout_Direction;

typedef LAYOUT_PACKED_ENUM {
    ALIGN_START,
    ALIGN_CENTER,
    ALIGN_END,
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
} Layout_TextSlice;

//TODO: text measurement results should probably be cached.
//TODO: create one internal line-breaking pass reused for height calculation and command emission
//TODO: maybe add different text wrapping policies?
//TODO: maybe add text alignment? can this be done by placing it in a container?
typedef struct {
    Layout_TextSlice text;
    Layout_TextStyle style;
} Layout_TextConfig;

#define LAYOUT_TEXT(s) ((Layout_TextSlice) {.items = (byte *)(s), .count = sizeof(s) - 1})

typedef struct {
    LAYOUT_PACKED_ENUM {
        LAYOUT_CMD_RECTANGLE,
        LAYOUT_CMD_TEXT,
        LAYOUT_CMD_CLIP_START,
        LAYOUT_CMD_CLIP_END,
        // LAYOUT_CMD_BORDER, // pass concrete codepoints to draw
    } type;
    union {
        struct CommandRectangle {
            i32 x, y, w, h;
            Layout_Color color;
        } rectangle;
        struct CommandText {
            i32 x, y;
            Layout_TextSlice text_slice;
            Layout_TextStyle style;
        } text;
        struct CommandClipStart {
            i32 x, y, w, h;
        } clip_start;
    } as;
} Layout_Command;

typedef struct CommandRectangle CommandRectangle;
typedef struct CommandText CommandText;
typedef struct CommandClipStart CommandClipStart;

typedef struct {
    Layout_Command *items;
    isize count;
} Layout_CommandSlice;

typedef i32 Layout_PersistentID;

void layout_screen_set_dimensions(i32 w, i32 h);
void layout_cursor_set_position(i32 x, i32 y);
void layout_scroll_update(i32 delta_y);
b32 layout_cursor_is_hovered(void);
Layout_PersistentID layout_cursor_get_hovered_id(void);
void layout_begin(void);
Layout_CommandSlice layout_end(void);
void layout_text_open(Layout_PersistentID id, Layout_TextConfig conf);
void layout_container_open(Layout_PersistentID id, Layout_ContainerConfig conf);
void layout_close(void);

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
