#ifndef LIBTUI_TUI_INCLUDE
#define LIBTUI_TUI_INCLUDE

#include "layla.h"
#include "brenda.h"

enum {
    TUI_ELEMENT_HOVERABLE      = 1 << 0,
    TUI_ELEMENT_CLICKABLE      = 1 << 1,
    TUI_ELEMENT_FOCUSABLE      = 1 << 2,
    TUI_ELEMENT_ACCEPTS_SCROLL = 1 << 3,
    TUI_ELEMENT_DISABLED       = 1 << 4,
};

// Returning true indicates that command was handled.
typedef b32 (*Tui_CommandHandler)(Layla_Command command, void *userdata);

typedef struct {
    Brenda_TerminalConfig terminal;
    i32 fps;
    Tui_CommandHandler command_handler;
    void *command_handler_userdata;
} Tui_Config;

typedef struct {
    u8 flags;
} Tui_ElementConfig;

typedef struct {
    Layla_ElementID id;
    Layla_ContainerStyle style;
    Layla_Floating floating;
    void *custom;
    u8 flags;
} Tui_DivConfig;

typedef struct {
    Layla_ElementID id;
    Layla_TextSlice text;
    Layla_TextStyle style;
    void *userdata;
    u8 flags;
} Tui_TextConfig;

typedef struct {
    Layla_ElementID id;
    Layla_TextSlice text;
    Layla_TextStyle text_style;
    Layla_ContainerStyle style;
    Layla_Color hovered_background;
    Layla_Color pressed_background;
    Layla_Color focused_background;
    b32 disabled;
} Tui_ButtonConfig;

typedef struct {
    byte *items;
    isize count;
    isize capacity;
    isize cursor;
} Tui_TextInputState;

typedef struct {
    Layla_ElementID id;
    Tui_TextInputState *state;
    Layla_TextSlice placeholder;
    Layla_ContainerStyle style;
    Layla_TextStyle text_style;
    Layla_TextStyle placeholder_style;
    Layla_Color focused_background;
    b32 disabled;
} Tui_TextInputConfig;

typedef struct {
    b32 changed;
    b32 submitted;
} Tui_TextInputResult;

void tui_init(Tui_Config config);
void tui_deinit(void);
// Begins both the Brenda and Layla frames. The returned events remain available to application code.
Brenda_EventSlice tui_frame_begin(void);
// Ends Layla, draws its commands through Brenda, and ends the Brenda frame.
void tui_frame_end(void);

void tui_element_register(Layla_ElementID id, Tui_ElementConfig config);
b32 tui_element_is_hovered(Layla_ElementID id);
b32 tui_element_is_pressed(Layla_ElementID id);
b32 tui_element_is_clicked(Layla_ElementID id);
b32 tui_element_is_focused(Layla_ElementID id);
void tui_element_focus(Layla_ElementID id);
Layla_ElementID tui_element_get_focused_id(void);

void tui_div_open(Tui_DivConfig config);
void tui_text_draw(Tui_TextConfig config);
b32 tui_button_draw(Tui_ButtonConfig config);
Tui_TextInputResult tui_text_input_draw(Tui_TextInputConfig config);
void tui_text_input_state_set_text(Tui_TextInputState *state, Layla_TextSlice text);

#define Tui_Div(...)                                                                                  \
    for (u8 _tui_latch = (tui_div_open((Tui_DivConfig) {                                             \
        .style.size.w = LAYLA_FIT(),                                                                 \
        .style.size.h = LAYLA_FIT(),                                                                 \
        __VA_ARGS__                                                                                  \
    }), 0); _tui_latch < 1; _tui_latch = 1, layla_element_close())

#define Tui_Text(...) tui_text_draw((Tui_TextConfig) {                                               \
    .style.color = LAYLA_COLOR(255, 255, 255),                                                       \
    __VA_ARGS__                                                                                      \
})

#define Tui_Button(...) tui_button_draw((Tui_ButtonConfig) {                                         \
    .style = {                                                                                       \
        .size = {.w = LAYLA_FIT(), .h = LAYLA_FIT()},                                               \
        .background = LAYLA_COLOR(70, 90, 180),                                                      \
        .padding = {.left = 1, .right = 1},                                                          \
        .border = {.width = 1, .color = LAYLA_COLOR(255, 255, 255)},                                 \
    },                                                                                               \
    .text_style = {.color = LAYLA_COLOR(255, 255, 255), .alignment = LAYLA_ALIGN_CENTER},            \
    .hovered_background = LAYLA_COLOR(100, 120, 220),                                                \
    .pressed_background = LAYLA_COLOR(45, 60, 130),                                                  \
    .focused_background = LAYLA_COLOR(100, 120, 220),                                                \
    __VA_ARGS__                                                                                      \
})

#define Tui_TextInput(...) tui_text_input_draw((Tui_TextInputConfig) {                               \
    .style = {                                                                                       \
        .size = {.w = LAYLA_FILL(.min = 8), .h = LAYLA_FIT()},                                      \
        .background = LAYLA_COLOR(30, 30, 30),                                                       \
        .padding = {.left = 1, .right = 1},                                                          \
        .border = {.width = 1, .color = LAYLA_COLOR(140, 140, 140)},                                 \
        .direction = LAYLA_DIR_ROW,                                                                  \
        .overflow = LAYLA_OVERFLOW_HIDDEN,                                                           \
    },                                                                                               \
    .text_style = {.color = LAYLA_COLOR(255, 255, 255)},                                             \
    .placeholder_style = {.color = LAYLA_COLOR(120, 120, 120)},                                     \
    .focused_background = LAYLA_COLOR(45, 45, 45),                                                   \
    __VA_ARGS__                                                                                      \
})

#endif
