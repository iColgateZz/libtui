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

typedef enum {
    TUI_BINDING_USE_DEFAULT,
    TUI_BINDING_DISABLED,
    TUI_BINDING_TERM_KEY,
    TUI_BINDING_CHARACTER,
} Tui_BindingType;

typedef struct {
    Tui_BindingType type;
    u8 modifiers;
    union {
        Brenda_TermKey term_key;
        byte character;
    } as;
} Tui_Binding;

#define TUI_BINDING_KEY(key_value, modifier_flags) \
    ((Tui_Binding) {.type = TUI_BINDING_TERM_KEY, .modifiers = (modifier_flags), .as.term_key = (key_value)})
#define TUI_BINDING_CHAR(character_value, modifier_flags) \
    ((Tui_Binding) {.type = TUI_BINDING_CHARACTER, .modifiers = (modifier_flags), .as.character = (character_value)})
#define TUI_BINDING_NONE ((Tui_Binding) {.type = TUI_BINDING_DISABLED})

typedef struct {
    Tui_Binding focus_next;
    Tui_Binding focus_previous;
    Tui_Binding focus_clear;
    Tui_Binding activate;
    Tui_Binding activate_alternate;
} Tui_Bindings;

typedef struct {
    Brenda_TerminalConfig terminal;
    i32 fps;
    Tui_Bindings bindings;
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
    Layla_ElementID target_id;
    Brenda_Event event;
} Tui_Event;

typedef struct {
    Tui_Event *items;
    isize count;
} Tui_EventSlice;

void tui_init(Tui_Config config);
void tui_deinit(void);
// Returns events that TUI did not consume. The slice remains valid until the next frame begins.
Tui_EventSlice tui_begin_frame(void);
// Ends Layla, draws its commands through Brenda, and ends the Brenda frame.
void tui_end_frame(void);

void tui_register_element(Layla_ElementID id, Tui_ElementConfig config);
b32 tui_is_element_hovered(Layla_ElementID id);
b32 tui_is_element_pressed(Layla_ElementID id);
b32 tui_is_element_clicked(Layla_ElementID id);
b32 tui_is_element_focused(Layla_ElementID id);
void tui_focus_element(Layla_ElementID id);
Layla_ElementID tui_get_focused_element_id(void);

void tui_open_div(Tui_DivConfig config);
void tui_draw_text(Tui_TextConfig config);
b32 tui_draw_button(Tui_ButtonConfig config);

#define Tui_Div(...)                                                                                  \
    for (u8 _tui_latch = (tui_open_div((Tui_DivConfig) {                                             \
        .style.size.w = LAYLA_FIT(),                                                                 \
        .style.size.h = LAYLA_FIT(),                                                                 \
        __VA_ARGS__                                                                                  \
    }), 0); _tui_latch < 1; _tui_latch = 1, layla_close_element())

#define Tui_Text(...) tui_draw_text((Tui_TextConfig) {                                               \
    .style.color = LAYLA_COLOR(255, 255, 255),                                                       \
    __VA_ARGS__                                                                                      \
})

#define Tui_Button(...) tui_draw_button((Tui_ButtonConfig) {                                         \
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

#endif
