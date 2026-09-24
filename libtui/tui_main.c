#define PSH_CORE_NO_PREFIX
#define PSH_CORE_IMPL
    #include "psh_core.h"
#undef PSH_CORE_IMPL
#undef PSH_CORE_NO_PREFIX

#include "tui.h"

enum {
    TUI_EXAMPLE_INCREMENT_ID = 1,
    TUI_EXAMPLE_QUIT_ID,
    TUI_EXAMPLE_TEXT_INPUT_ID,
    TUI_EXAMPLE_SCROLL_ID,
    TUI_EXAMPLE_PURPLE_PANEL_ID,
    TUI_EXAMPLE_GREEN_PANEL_ID,
};

void draggable_panel(i32 id, i32 z_index, Layla_Color color, Layla_TextSlice text) {
    Tui_Div(
        .id = id,
        .floating = {
            .draggable = true,
            .attach_to = {.type = LAYLA_ATTACH_TO_ROOT},
            .z_index = z_index,
            .parent_attach_point =  {.x = LAYLA_ALIGN_END, .y = LAYLA_ALIGN_START},
            .element_attach_point = {.x = LAYLA_ALIGN_END, .y = LAYLA_ALIGN_START},
        },
        .style = {
            .size = {.w = LAYLA_FIXED(26), .h = LAYLA_FIXED(4)},
            .background = color,
            .padding = {.left = 1, .right = 1, .top = 1},
            .border = {.width = 1, .color = LAYLA_COLOR(210, 160, 255)},
        },
    ) {
        Tui_Text(.text = text);
    }
}

i32 main(void) {
    tui_init((Tui_Config) {.fps = 60});

    i32 count = 0;
    i32 submission_count = 0;
    b32 quit = false;
    byte input_text[256] = "";
    Tui_TextInputState input = tui_make_text_input_state(input_text, sizeof(input_text));

    while (!quit) {
        tui_begin_frame();

        byte count_text[64] = {0};
        i32 count_text_length = snprintf(count_text, sizeof(count_text), "Button clicks: %d; submissions: %d", count, submission_count);

        Tui_Div(.style = {
            .size = {.w = LAYLA_FILL(), .h = LAYLA_FILL()},
            .background = LAYLA_COLOR(20, 24, 32),
            .padding = {.left = 2, .right = 2, .top = 1, .bottom = 1},
            .spacing = 1,
            .direction = LAYLA_DIR_COL,
        }) {
            Tui_Text(
                .text = LAYLA_TEXT_SLICE("TUI high-level example"), 
                .style = {
                    .color = LAYLA_COLOR(140, 190, 255),
                }
            );

            if (Tui_Button(.id = TUI_EXAMPLE_INCREMENT_ID, .text = LAYLA_TEXT_SLICE("Increment"))) count++;
            if (Tui_Button(.id = TUI_EXAMPLE_QUIT_ID, .text = LAYLA_TEXT_SLICE("Quit"))) quit = true;

            Tui_Text(.text = ((Layla_TextSlice) {.items = count_text, .count = count_text_length}));

            Tui_Text(.text = LAYLA_TEXT_SLICE("Text input (Enter: new row; Ctrl+S: submit; Escape: release focus):"));
            Tui_TextInputResult input_result = Tui_TextInput(
                .id = TUI_EXAMPLE_TEXT_INPUT_ID,
                .state = &input,
                .placeholder = LAYLA_TEXT_SLICE("Click here and type. Long text wraps inside the input."),
                .submit_binding = TUI_BINDING_CHAR('s', BRENDA_MODIFIER_CTRL),
            );
            if (input_result.submitted) submission_count++;

            Tui_Div(
                .id = TUI_EXAMPLE_SCROLL_ID,
                .style = {
                    .size = {.w = LAYLA_FILL(), .h = LAYLA_FIXED(5)},
                    .background = LAYLA_COLOR(35, 42, 55),
                    .padding = {.left = 1, .right = 1},
                    .border = {.width = 1, .color = LAYLA_COLOR(90, 105, 130)},
                    .direction = LAYLA_DIR_COL,
                    .scroll = LAYLA_SCROLL_Y,
                },
            ) {
                Tui_Text(.text = LAYLA_TEXT_SLICE("This panel scrolls with the mouse wheel."));
                Tui_Text(.text = LAYLA_TEXT_SLICE("TUI updates the Layla scroll offset automatically."));
                Tui_Text(.text = LAYLA_TEXT_SLICE("Tab and Shift+Tab move keyboard focus."));
                Tui_Text(.text = LAYLA_TEXT_SLICE("Enter and Space activate focused buttons."));
                Tui_Text(.text = LAYLA_TEXT_SLICE("Unhandled Brenda events remain available to the application."));
                Tui_Text(.text = LAYLA_TEXT_SLICE("The Layla command adapter is also automatic."));
            }

            draggable_panel(TUI_EXAMPLE_PURPLE_PANEL_ID, 10, LAYLA_COLOR(90, 55, 120), LAYLA_TEXT_SLICE("Drag me with the mouse"));
            draggable_panel(TUI_EXAMPLE_GREEN_PANEL_ID,  11, LAYLA_COLOR(35, 100, 80), LAYLA_TEXT_SLICE("Drag this panel too"));
        }

        Tui_EventSlice events = tui_get_unhandled_events();
        for (isize i = 0; i < events.count; ++i) {
            Brenda_Event input_event = events.items[i].input;
            if (input_event.type == BRENDA_EVENT_UTF8 && input_event.as.utf8.length == 1
                && input_event.as.utf8.bytes[0] == 'q') {
                quit = true;
            }
        }

        tui_end_frame();
    }

    tui_deinit();
    return 0;
}
