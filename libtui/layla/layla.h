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
    #ifndef LAYLA_MAX_ERRORS
        #define LAYLA_MAX_ERRORS 128
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

#define LAYLA_PERCENT(percent_value, ...) ((Layla_SizeStyle) {  \
    .type = LAYLA_SIZE_PERCENT,                                 \
    .as.percent = {                                             \
        .value = (percent_value),                               \
        .min = 0,                                               \
        .max = INT32_MAX,                                       \
        __VA_ARGS__                                             \
    },                                                          \
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
} Layla_RGB;

#define LAYLA_RGB(r, g, b) ((Layla_RGB) {(r), (g), (b)})

typedef struct {
    Layla_RGB color;
    b32 is_set;
} Layla_Color;

#define LAYLA_COLOR(r, g, b) ((Layla_Color) { \
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
    Layla_RGB color;
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

typedef u32 Layla_ElementID;
#define LAYLA_ELEMENT_ID_NONE 0

typedef struct {
    Layla_Rectangle rectangle;
    b32 found;
} Layla_ElementData;

typedef struct {
    Layla_ElementID *items;
    isize count;
} Layla_ElementIDSlice;

typedef LAYLA_PACKED_ENUM {
    LAYLA_CURSOR_RELEASED,
    LAYLA_CURSOR_PRESSED_THIS_FRAME,
    LAYLA_CURSOR_PRESSED,
    LAYLA_CURSOR_RELEASED_THIS_FRAME,
} Layla_CursorInteractionState;

typedef struct {
    i32 x, y;
    Layla_CursorInteractionState interaction_state;
} Layla_CursorState;

typedef LAYLA_PACKED_ENUM {
    LAYLA_SCROLL_NONE,
    LAYLA_SCROLL_Y,
} Layla_ScrollAxis;

typedef LAYLA_PACKED_ENUM {
    LAYLA_OVERFLOW_HIDDEN,
    LAYLA_OVERFLOW_VISIBLE,
} Layla_Overflow;

typedef struct {
    Layla_Alignment x;
    Layla_Alignment y;
} Layla_FloatingAttachPoint;

typedef struct {
    LAYLA_PACKED_ENUM {
        LAYLA_ATTACH_TO_NONE,
        LAYLA_ATTACH_TO_PARENT,
        LAYLA_ATTACH_TO_ROOT,
        LAYLA_ATTACH_TO_ELEMENT,
    } type;
    union {
        struct {
            Layla_ElementID id;
        } element;
    } as;
} Layla_FloatingAttachTo;

typedef struct {
    Layla_FloatingAttachTo attach_to;
    struct {
        Layla_FloatingAttachPoint parent;
        Layla_FloatingAttachPoint element;
    } attach_point;
    i32 z_index;
} Layla_Floating;

typedef struct {
    Layla_Sizing size;
    Layla_Color background;
    Layla_Padding padding;
    Layla_BorderStyle border;
    u8 spacing;
    Layla_Direction direction;
    Layla_Alignment align_children;
    Layla_Alignment align_self;
    Layla_ScrollAxis scroll;
    Layla_Overflow overflow;
} Layla_ContainerStyle;

typedef struct {
    Layla_ContainerStyle style;
    Layla_Floating floating;
    void *custom;
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
    Layla_RGB color;
    Layla_Alignment alignment;
    Layla_TextWrapPolicy wrap_policy;
} Layla_TextStyle;

typedef struct {
    Layla_TextSlice text;
    Layla_TextStyle style;
    void *userdata;
} Layla_TextConfig;

typedef LAYLA_PACKED_ENUM {
    LAYLA_ERROR_SCREEN_DIMENSIONS_NOT_SET,
    LAYLA_ERROR_TEXT_MEASURE_FUNCTION_NOT_SET,
    LAYLA_ERROR_TEXT_MEASURE_RETURNED_NEGATIVE_WIDTH,
} Layla_ErrorType;

typedef struct {
    Layla_ErrorType type;
    Layla_ElementID id;
    byte const *message;
} Layla_Error;

typedef struct {
    Layla_Error *items;
    isize count;
} Layla_ErrorSlice;

typedef void (*Layla_ErrorHandler)(Layla_Error error, void *userdata);

typedef struct {
    LAYLA_PACKED_ENUM {
        LAYLA_CMD_RECTANGLE,
        LAYLA_CMD_TEXT,
        LAYLA_CMD_CLIP_START,
        LAYLA_CMD_CLIP_END,
        LAYLA_CMD_BORDER,
        LAYLA_CMD_CUSTOM,
    } type;
    Layla_ElementID id;
    union {
        struct Layla_CommandRectangle {
            i32 x, y, w, h;
            Layla_RGB color;
        } rectangle;
        struct Layla_CommandText {
            i32 x, y;
            Layla_TextSlice slice;
            Layla_RGB color;
            void *userdata;
        } text;
        struct Layla_CommandClipStart {
            i32 x, y, w, h;
        } clip_start;
        struct Layla_CommandBorder {
            i32 x, y, w, h;
            Layla_RGB color;
            void *userdata;
        } border;
        struct Layla_CommandCustom {
            i32 x, y, w, h;
            void *userdata;
        } custom;
    } as;
} Layla_Command;

typedef struct Layla_CommandRectangle Layla_CommandRectangle;
typedef struct Layla_CommandText Layla_CommandText;
typedef struct Layla_CommandClipStart Layla_CommandClipStart;
typedef struct Layla_CommandBorder Layla_CommandBorder;
typedef struct Layla_CommandCustom Layla_CommandCustom;

typedef struct {
    Layla_Command *items;
    isize count;
} Layla_CommandSlice;

// Return the width of the borrowed UTF-8 span in layout units.
// The function must handle empty spans and return a non-negative value.
typedef i32 (*Layla_TextMeasureFunction)(Layla_TextSlice text, void *userdata);

void layla_state_set_error_handler(Layla_ErrorHandler handler, void *userdata);
Layla_ErrorSlice layla_state_get_errors(void);

void layla_state_set_text_measure_function(Layla_TextMeasureFunction function, void *userdata);
void layla_state_set_screen_dimensions(i32 w, i32 h);
// Call once per frame before layla_layout_begin(). Hit-tests the last completed layout and updates interaction_state.
void layla_state_set_cursor_state(i32 x, i32 y, b32 is_down);
Layla_CursorState layla_state_get_cursor_state(void);

void layla_scroll_offset_update_on_hovered_element(i32 delta_y);
void layla_scroll_offset_set_by_id(Layla_ElementID id, i32 offset_y);
void layla_scroll_offset_update_by_id(Layla_ElementID id, i32 delta_y);
i32 layla_scroll_offset_get_by_id(Layla_ElementID id);
i32 layla_scroll_max_offset_get_by_id(Layla_ElementID id);

b32 layla_state_is_element_hovered(void);
b32 layla_state_is_element_hovered_by_id(Layla_ElementID id);
// The returned slice remains valid until the cursor state is set again.
Layla_ElementIDSlice layla_state_get_hovered_element_ids(void);
Layla_ElementID layla_element_get_open_id(void);
// Returns data from the last completed layout. During layout construction, this is the preceding frame's data.
Layla_ElementData layla_element_data_get_by_id(Layla_ElementID id);

void layla_layout_begin(void);
Layla_CommandSlice layla_layout_end(void);

void layla_text_element_open(void);
void layla_text_element_open_with_id(Layla_ElementID id);
void layla_text_element_configure(Layla_TextConfig conf);
void layla_container_element_open(void);
void layla_container_element_open_with_id(Layla_ElementID id);
void layla_container_element_configure(Layla_ContainerConfig conf);
void layla_element_close(void);

// Automatic IDs depend on the parent ID and sibling declaration order.
#define Layla_Container(...)                                    \
    for (u8 _latch = (layla_container_element_open(),           \
        layla_container_element_configure(                      \
            (Layla_ContainerConfig) {                           \
                .style.size.w = LAYLA_FIT(),                    \
                .style.size.h = LAYLA_FIT(),                    \
                __VA_ARGS__                                     \
        }), 0); _latch < 1; _latch = 1, layla_element_close())

#define Layla_ContainerID(id, ...)                              \
    for (u8 _latch = (layla_container_element_open_with_id(id), \
        layla_container_element_configure(                      \
            (Layla_ContainerConfig) {                           \
                .style.size.w = LAYLA_FIT(),                    \
                .style.size.h = LAYLA_FIT(),                    \
                __VA_ARGS__                                     \
        }), 0); _latch < 1; _latch = 1, layla_element_close())

#define Layla_Text(...)                                         \
    for (u8 _latch = (layla_text_element_open(),                \
        layla_text_element_configure(                           \
            (Layla_TextConfig) {__VA_ARGS__}                    \
        ), 0); _latch < 1; _latch = 1, layla_element_close())

#define Layla_TextID(id, ...)                                   \
    for (u8 _latch = (layla_text_element_open_with_id(id),      \
        layla_text_element_configure(                           \
            (Layla_TextConfig) {__VA_ARGS__}                    \
        ), 0); _latch < 1; _latch = 1, layla_element_close())

#endif
