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
    // LAYLA_MAX_NODES and LAYLA_MAX_SCROLL_STATES must be powers of two.
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
    u8 is_set;
} Layla_Color;

#define LAYLA_COLOR(red, green, blue) \
    ((Layla_Color) {.r = (red), .g = (green), .b = (blue), .is_set = true})

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

typedef u32 Layla_ElementID;
#define LAYLA_ELEMENT_ID_NONE 0

enum {
    LAYLA_ELEMENT_CONTAINER = 1 << 0,
    LAYLA_ELEMENT_TEXT      = 1 << 1,
    LAYLA_ELEMENT_SCROLL_Y  = 1 << 2,
    LAYLA_ELEMENT_FLOATING  = 1 << 3,
};

typedef struct {
    Layla_ElementID id;
    Layla_ElementID parent_id;
    Layla_Rectangle rectangle;
    u8 flags;
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

typedef LAYLA_PACKED_ENUM {
    // Stop hit testing after this floating tree is hit.
    LAYLA_CURSOR_CAPTURE,
    // Keep this floating tree's hover results and continue hit testing underneath it.
    LAYLA_CURSOR_FALLTHROUGH,
} Layla_CursorCaptureMode;

typedef struct {
    Layla_FloatingAttachTo attach_to;
    struct {
        Layla_FloatingAttachPoint parent;
        Layla_FloatingAttachPoint element;
    } attach_point;
    Layla_CursorCaptureMode cursor_capture_mode;
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

// Return the width of the borrowed UTF-8 span in layout units.
// The function must handle empty spans and return a non-negative value.
typedef i32 (*Layla_TextMeasureFunction)(Layla_TextSlice text, void *userdata);

typedef LAYLA_PACKED_ENUM {
    LAYLA_TEXT_WRAP_WORD,
    LAYLA_TEXT_WRAP_CHARACTER,
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
            Layla_Color color;
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

void layla_set_error_handler(Layla_ErrorHandler handler, void *userdata);
Layla_ErrorSlice layla_get_errors(void);

void layla_set_text_measure_function(Layla_TextMeasureFunction function, void *userdata);
void layla_set_screen_dimensions(i32 w, i32 h);

// Call once per frame before layla_begin_layout(). Hit-tests the last completed layout and updates interaction_state.
void layla_set_cursor_state(i32 x, i32 y, b32 is_down);
Layla_CursorState layla_get_cursor_state(void);

void layla_set_scroll_offset(Layla_ElementID id, i32 offset_y);
void layla_update_scroll_offset(Layla_ElementID id, i32 delta_y);
i32 layla_get_scroll_offset(Layla_ElementID id);
i32 layla_get_max_scroll_offset(Layla_ElementID id);

b32 layla_is_open_element_hovered(void);
b32 layla_is_element_hovered(Layla_ElementID id);
// The returned IDs are ordered back to front.
// The slice remains valid until the cursor state is set again.
Layla_ElementIDSlice layla_get_hovered_element_ids(void);

Layla_ElementID layla_get_open_element_id(void);
// Returns data from the last completed layout. During layout construction, this is the preceding frame's data.
Layla_ElementData layla_get_element_data(Layla_ElementID id);

void layla_begin_layout(void);
Layla_CommandSlice layla_end_layout(void);

void layla_open_text_element(void);
void layla_open_text_element_with_id(Layla_ElementID id);
void layla_configure_text_element(Layla_TextConfig conf);
void layla_open_container_element(void);
void layla_open_container_element_with_id(Layla_ElementID id);
void layla_configure_container_element(Layla_ContainerConfig conf);
void layla_close_element(void);

// Automatic IDs depend on the parent ID and sibling declaration order.
#define Layla_Container(...)                                    \
    for (u8 _latch = (layla_open_container_element(),           \
        layla_configure_container_element(                      \
            (Layla_ContainerConfig) {                           \
                .style.size.w = LAYLA_FIT(),                    \
                .style.size.h = LAYLA_FIT(),                    \
                __VA_ARGS__                                     \
        }), 0); _latch < 1; _latch = 1, layla_close_element())

#define Layla_ContainerID(id, ...)                              \
    for (u8 _latch = (layla_open_container_element_with_id(id), \
        layla_configure_container_element(                      \
            (Layla_ContainerConfig) {                           \
                .style.size.w = LAYLA_FIT(),                    \
                .style.size.h = LAYLA_FIT(),                    \
                __VA_ARGS__                                     \
        }), 0); _latch < 1; _latch = 1, layla_close_element())

#define Layla_Text(...) do {                                        \
    layla_open_text_element();                                      \
    layla_configure_text_element((Layla_TextConfig) {__VA_ARGS__}); \
    layla_close_element();                                          \
} while (0)

#define Layla_TextID(id, ...) do {                                  \
    layla_open_text_element_with_id(id);                            \
    layla_configure_text_element((Layla_TextConfig) {__VA_ARGS__}); \
    layla_close_element();                                          \
} while (0)

#endif
