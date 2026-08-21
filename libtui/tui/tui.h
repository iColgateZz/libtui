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
    TUI_ELEMENT_DRAGGABLE      = 1 << 5,
    TUI_ELEMENT_TEXT_INPUT     = 1 << 6,
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
    b32 draggable;
    Layla_FloatingAttachTo attach_to;
    struct {
        Layla_FloatingAttachPoint parent;
        Layla_FloatingAttachPoint element;
    } attach_point;
    Layla_CursorCaptureMode cursor_capture_mode;
    i32 z_index;
} Tui_Floating;

typedef struct {
    u8 flags;
} Tui_ElementConfig;

typedef struct {
    Layla_ElementID id;
    Layla_ContainerStyle style;
    Tui_Floating floating;
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

// Return true when the event was handled and should not be exposed to the application.
typedef b32 (*Tui_EventHandler)(Tui_Event event, void *userdata);

typedef enum {
    TUI_DRAG_NONE,
    TUI_DRAG_STARTED,
    TUI_DRAGGING,
    TUI_DRAG_RELEASED,
} Tui_DragInteractionState;

typedef struct {
    Layla_ElementID element_id;
    Tui_DragInteractionState interaction_state;
    // Element position at the start of the current drag.
    i32 start_x, start_y;
    // Total displacement from the drag start position.
    i32 delta_x, delta_y;
} Tui_DragState;

typedef struct {
    byte *items;
    isize count;
    isize capacity;
    isize cursor;
} Tui_TextInputState;

typedef struct {
    Layla_ElementID id;
    Tui_TextInputState *state;
    Layla_ContainerStyle style;
    Layla_TextStyle text_style;
    Layla_Color focused_background;
    b32 disabled;
} Tui_TextInputConfig;

void tui_init(Tui_Config config);
void tui_deinit(void);
void tui_begin_frame(void);
// Ends Layla, draws its commands through Brenda, and ends the Brenda frame.
void tui_end_frame(void);

// Widgets call this while being declared to consume events routed to their element ID.
void tui_consume_element_specific_events(Layla_ElementID id, Tui_EventHandler handler, void *userdata);
// Call after declaring widgets. The slice remains valid until the next call or frame begins.
Tui_EventSlice tui_get_unhandled_events(void);

void tui_register_element(Layla_ElementID id, Tui_ElementConfig config);
b32 tui_is_element_hovered(Layla_ElementID id);
b32 tui_is_element_pressed(Layla_ElementID id);
b32 tui_is_element_clicked(Layla_ElementID id);
b32 tui_is_element_focused(Layla_ElementID id);
void tui_focus_element(Layla_ElementID id);
Layla_ElementID tui_get_focused_element_id(void);
Tui_DragState tui_get_drag_state(Layla_ElementID id);

void tui_open_div(Tui_DivConfig config);
void tui_draw_text(Tui_TextConfig config);
b32 tui_draw_button(Tui_ButtonConfig config);
b32 tui_draw_text_input(Tui_TextInputConfig config);

#define Tui_Div(...)                                                                                 \
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
        .size = {.w = LAYLA_FIT(), .h = LAYLA_FIT()},                                                \
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

#define Tui_TextInput(...) tui_draw_text_input((Tui_TextInputConfig) {                               \
    .style = {                                                                                       \
        .size = {.w = LAYLA_FILL(), .h = LAYLA_FIXED(5)},                                            \
        .background = LAYLA_COLOR(35, 42, 55),                                                       \
        .padding = {.left = 1, .right = 1},                                                          \
        .border = {.width = 1, .color = LAYLA_COLOR(90, 105, 130)},                                  \
        .direction = LAYLA_DIR_COL,                                                                  \
    },                                                                                               \
    .text_style = {                                                                                  \
        .color = LAYLA_COLOR(255, 255, 255),                                                         \
        .wrap_policy = LAYLA_TEXT_WRAP_CHARACTER,                                                    \
    },                                                                                               \
    .focused_background = LAYLA_COLOR(45, 55, 72),                                                   \
    __VA_ARGS__                                                                                      \
})

#endif
