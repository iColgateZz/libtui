#ifndef LIBTUI_LAYLA_INCLUDE
#define LIBTUI_LAYLA_INCLUDE

#include "psh_core.h"

#if defined(__has_attribute)
    #define LAYLA_HAS_ATTRIBUTE(attribute) __has_attribute(attribute)
#else
    #define LAYLA_HAS_ATTRIBUTE(attribute) 0
#endif

#if LAYLA_HAS_ATTRIBUTE(packed) || defined(__GNUC__) || defined(__clang__)
    #define LAYLA_PACKED_ENUM enum __attribute__((__packed__))
#else
    #define LAYLA_PACKED_ENUM enum
#endif

#ifdef LAYLA_STATIC_STORAGE
    #ifndef LAYLA_MAX_NODES
        #define LAYLA_MAX_NODES 1024
    #endif
    #ifndef LAYLA_MAX_COMMANDS
        #define LAYLA_MAX_COMMANDS 1024
    #endif
    #ifndef LAYLA_MAX_SCROLL_STATES
        #define LAYLA_MAX_SCROLL_STATES 1024
    #endif
    #ifndef LAYLA_TEMP_STORAGE_SIZE
        #define LAYLA_TEMP_STORAGE_SIZE (64 * 1024)
    #endif
#endif

typedef struct {
    LAYLA_PACKED_ENUM {
        LAYLA_SIZE_FIT,
        LAYLA_SIZE_FILL,
        LAYLA_SIZE_FIXED,
        LAYLA_SIZE_PERCENT,
    } type;
    union {
        struct {
            i32 value;
        } fixed;
        struct {
            f32 value;
            i32 min;
            i32 max;
        } percent;
        struct {
            i32 min;
            i32 max;
        } fit;
        struct {
            i32 min;
            i32 max;
        } fill;
    } as;
} Layla_SizeStyle;

#define LAYLA_FIXED(fixed_value) ((Layla_SizeStyle) {          \
    .type = LAYLA_SIZE_FIXED,                                  \
    .as.fixed = {.value = (fixed_value)},                      \
})

#define LAYLA_PERCENT(percent, ...) ((Layla_SizeStyle) {    \
    .type = LAYLA_SIZE_PERCENT,                             \
    .as.percent = {                                         \
        .value = (percent),                                 \
        .min = 0,                                           \
        .max = INT32_MAX,                                   \
        __VA_ARGS__,                                        \
    },                                                      \
})

#define LAYLA_FIT(...) ((Layla_SizeStyle) { \
    .type = LAYLA_SIZE_FIT,                 \
    .as.fit = {                             \
        .min = 0,                           \
        .max = INT32_MAX,                   \
        __VA_ARGS__                         \
    },                                      \
})

#define LAYLA_FILL(...) ((Layla_SizeStyle) {    \
    .type = LAYLA_SIZE_FILL,                    \
    .as.fill = {                                \
        .min = 0,                               \
        .max = INT32_MAX,                       \
        __VA_ARGS__                             \
    },                                          \
})

typedef struct {
    Layla_SizeStyle w;
    Layla_SizeStyle h;
} Layla_Sizing;

typedef struct {
    u8 r, g, b;
} Layla_Color;

#define LAYLA_RGB(r, g, b) ((Layla_Color) {(r), (g), (b)})

typedef struct {
    Layla_Color color;
    b32 is_set;
} Layla_Background;

#define LAYLA_BACKGROUND(r, g, b) ((Layla_Background) { \
    .color = LAYLA_RGB((r), (g), (b)),                  \
    .is_set = true,                                     \
})

typedef struct {
    i32 x, y, w, h;
} Layla_Rectangle;

typedef struct {
    u8 left;
    u8 right;
    u8 top;
    u8 bottom;
} Layla_Padding;

typedef struct {
    u8 width;
    Layla_Color color;
    void *userdata;
} Layla_BorderStyle;

typedef LAYLA_PACKED_ENUM {
    LAYLA_DIR_ROW,
    LAYLA_DIR_COL,
} Layla_Direction;

typedef LAYLA_PACKED_ENUM {
    LAYLA_ALIGN_START,
    LAYLA_ALIGN_CENTER,
    LAYLA_ALIGN_END,
} Layla_Alignment;

typedef LAYLA_PACKED_ENUM {
    LAYLA_SCROLL_NONE,
    LAYLA_SCROLL_Y,
} Layla_Scroll;

typedef LAYLA_PACKED_ENUM {
    LAYLA_OVERFLOW_HIDDEN,
    LAYLA_OVERFLOW_VISIBLE,
} Layla_Overflow;

typedef struct {
    Layla_Sizing size;
    Layla_Background background;
    Layla_Padding padding;
    Layla_BorderStyle border;
    u8 spacing;
    Layla_Direction direction;
    Layla_Alignment align_children;
    Layla_Alignment align_self;
    Layla_Scroll scroll;
    Layla_Overflow overflow;
} Layla_ContainerStyle;

typedef struct {
    Layla_ContainerStyle style;
    void *userdata;
} Layla_ContainerConfig;

typedef struct {
    byte *items;
    isize count;
} Layla_TextSlice;

#define LAYLA_TEXT_SLICE(s) ((Layla_TextSlice) {.items = (byte *)(s), .count = sizeof(s) - 1})

typedef LAYLA_PACKED_ENUM {
    LAYLA_TEXT_WRAP_WORD,
} Layla_TextWrapPolicy;

typedef struct {
    Layla_Color color;
    Layla_Alignment alignment;
    Layla_TextWrapPolicy wrap_policy;
} Layla_TextStyle;

typedef struct {
    Layla_TextSlice text;
    Layla_TextStyle style;
    void *userdata;
} Layla_TextConfig;

typedef i32 Layla_PersistentID;
#define LAYLA_PERSISTENT_ID_NONE 0

typedef struct {
    LAYLA_PACKED_ENUM {
        LAYLA_CMD_RECTANGLE,
        LAYLA_CMD_TEXT,
        LAYLA_CMD_CLIP_START,
        LAYLA_CMD_CLIP_END,
        LAYLA_CMD_BORDER,
        //TODO: Custom container should be offset into the custom-user-zone
    } type;
    Layla_PersistentID id;
    union {
        struct Layla_CommandRectangle {
            i32 x, y, w, h;
            Layla_Color color;
            void *userdata;
        } rectangle;
        struct Layla_CommandText {
            i32 x, y;
            Layla_TextSlice slice;
            Layla_Color color;
            void *userdata;
        } text;
        struct Layla_CommandClipStart {
            i32 x, y, w, h;
        } clip_start;
        struct Layla_CommandBorder {
            i32 x, y, w, h;
            Layla_Color color;
            void *userdata;
        } border;
    } as;
} Layla_Command;

typedef struct Layla_CommandRectangle Layla_CommandRectangle;
typedef struct Layla_CommandText Layla_CommandText;
typedef struct Layla_CommandClipStart Layla_CommandClipStart;
typedef struct Layla_CommandBorder Layla_CommandBorder;

typedef struct {
    Layla_Command *items;
    isize count;
} Layla_CommandSlice;

// Return the width of the borrowed UTF-8 span in layout units.
// The function must handle empty spans and return a non-negative value.
typedef i32 (*Layla_TextMeasureFunction)(Layla_TextSlice text, void *userdata);

void layla_state_set_text_measure_function(Layla_TextMeasureFunction function, void *userdata);
void layla_state_set_screen_dimensions(i32 w, i32 h);
void layla_state_set_cursor_position(i32 x, i32 y);

void layla_state_update_scroll_offset_on_hovered_element(i32 delta_y);
void layla_state_set_scroll_offset_by_id(Layla_PersistentID id, i32 offset_y);
void layla_state_update_scroll_offset_by_id(Layla_PersistentID id, i32 delta_y);
i32 layla_state_get_scroll_offset_by_id(Layla_PersistentID id);
i32 layla_state_get_max_scroll_offset_by_id(Layla_PersistentID id);

b32 layla_state_is_element_hovered(void);
Layla_PersistentID layla_state_get_hovered_element_id(void);

void layla_layout_begin(void);
Layla_CommandSlice layla_layout_end(void);

void layla_text_element_open(Layla_PersistentID id, Layla_TextConfig conf);
void layla_container_element_open(Layla_PersistentID id, Layla_ContainerConfig conf);
void layla_element_close(void);

#define Layla_Container(id, ...)                                \
    for (u8 _latch = (layla_container_element_open(             \
            id,                                                 \
            (Layla_ContainerConfig) {                           \
                .style.size.w = LAYLA_FIT(),                    \
                .style.size.h = LAYLA_FIT(),                    \
                __VA_ARGS__                                     \
        }), 0); _latch < 1; _latch = 1, layla_element_close())

#define Layla_Text(id, ...)                                     \
    for (u8 _latch = (layla_text_element_open(                  \
            id,                                                 \
            (Layla_TextConfig) {__VA_ARGS__}                    \
        ), 0); _latch < 1; _latch = 1, layla_element_close())

#endif
