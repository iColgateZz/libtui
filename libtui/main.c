#define PSH_CORE_NO_PREFIX
#define PSH_CORE_IMPL
    #include "psh_core.h"
#undef PSH_CORE_IMPL
#undef PSH_CORE_NO_PREFIX

#include "layla.h"
#include "brenda.h"

enum {
    BUTTON_QUIT_ID = 1,
    SCROLL_PANEL_ID,
    TOOLTIP_ID,
};

static i32 text_measure(Layla_TextSlice text, void *userdata) {
    UNUSED(userdata);
    return brenda_text_measure_width(text.items, text.count);
}

static b32 button(Layla_ElementID id, Layla_TextSlice label) {
    b32 hovered = false;

    Layla_ContainerID(id, .style = {
        .background = layla_state_is_element_hovered()
            ? LAYLA_COLOR(120, 150, 255)
            : LAYLA_COLOR(70, 90, 180),
        .padding = {.left = 1, .right = 1},
        .border = {.width = 1, .color = LAYLA_RGB(255, 255, 255)},
        .align_self = LAYLA_ALIGN_CENTER,
        .size = {.w = LAYLA_FIT(), .h = LAYLA_FIT()},
    }) {
        hovered = layla_state_is_element_hovered();
        Layla_Text(.text = label, .style = {
            .color = LAYLA_RGB(255, 255, 255),
            .alignment = LAYLA_ALIGN_CENTER,
        });
    }

    Layla_CursorState cursor = layla_state_get_cursor_state();
    return hovered && cursor.interaction_state == LAYLA_CURSOR_RELEASED_THIS_FRAME;
}

i32 main(void) {
    layla_state_set_text_measure_function(text_measure, NULL);
    brenda_terminal_init();
    brenda_terminal_set_fps(60);

    b32 quit = false;
    b32 tooltip_open = false;
    while (!quit) {
        brenda_frame_begin();

        Layla_CursorState cursor = layla_state_get_cursor_state();
        b32 cursor_is_down = cursor.interaction_state == LAYLA_CURSOR_PRESSED_THIS_FRAME ||
                             cursor.interaction_state == LAYLA_CURSOR_PRESSED;
        i32 scroll_delta_y = 0;
        b32 left_mouse_pressed = false;
        b32 right_mouse_pressed = false;
        Brenda_EventSlice events = brenda_events_get();
        for (isize i = 0; i < events.count; ++i) {
            Brenda_Event event = events.items[i];
            if (event.type == BRENDA_EVENT_UTF8 
                && event.as.utf8.length == 1 && event.as.utf8.bytes[0] == 'q') quit = true;
            if (event.type < BRENDA_EVENT_MOUSE_LEFT) continue;

            cursor.x = event.as.mouse.x;
            cursor.y = event.as.mouse.y;
            if (event.type == BRENDA_EVENT_MOUSE_LEFT) {
                cursor_is_down = event.as.mouse.pressed;
                left_mouse_pressed = event.as.mouse.pressed;
            }
            if (event.type == BRENDA_EVENT_MOUSE_RIGHT && event.as.mouse.pressed) {
                right_mouse_pressed = true;
            }
            if (event.type == BRENDA_EVENT_MOUSE_DRAG) cursor_is_down = true;
            if (event.type == BRENDA_EVENT_SCROLL_UP) scroll_delta_y--;
            if (event.type == BRENDA_EVENT_SCROLL_DOWN) scroll_delta_y++;
        }
        if (left_mouse_pressed) tooltip_open = false;
        if (right_mouse_pressed) tooltip_open = layla_state_is_element_hovered_by_id(BUTTON_QUIT_ID);
        layla_state_set_cursor_state(cursor.x, cursor.y, cursor_is_down);
        layla_scroll_offset_update_on_hovered_element(scroll_delta_y);

        {
            u32 w = brenda_terminal_get_width();
            u32 h = brenda_terminal_get_height();
            layla_state_set_screen_dimensions(w, h);
            layla_layout_begin();

            Layla_Container(.style = {
                .background = LAYLA_COLOR(196, 240, 120),
                .direction = LAYLA_DIR_ROW,
                .size = {.w = LAYLA_FIXED(w), .h = LAYLA_FIXED(h)},
            }) {
                Layla_Container(.style = {
                    .size = {.w = LAYLA_FILL(.max = 10), .h = LAYLA_PERCENT(0.5)},
                    // .size = {.w = LAYLA_PERCENT(0.4), .h = LAYLA_PERCENT(0.5)},
                    .background = LAYLA_COLOR(255, 133, 182),
                    .align_self = LAYLA_ALIGN_CENTER,
                });

                Layla_ContainerID(SCROLL_PANEL_ID, .style = {
                    .size = {.w = LAYLA_PERCENT(.8), .h = LAYLA_FIT()},
                    .background = LAYLA_COLOR(233, 255, 57),
                    .align_self = LAYLA_ALIGN_CENTER,
                    .direction = LAYLA_DIR_COL,
                    .padding = {.left = 1, .right = 1, .top = 5, .bottom = 1},
                    .border = {.width = 1},
                    .scroll = LAYLA_SCROLL_Y,
                }) {
                    Layla_Text(.text = LAYLA_TEXT_SLICE("LibTUI text wraps inside containers. LibTUI text wraps inside containers."),
                        .style = {
                            .color = {255, 255, 255},
                            .alignment = LAYLA_ALIGN_CENTER,
                        },
                    );
                    Layla_Container(.style = {
                        .size = {.w = LAYLA_FILL(), .h = LAYLA_FIXED(5)},
                        .background = LAYLA_COLOR(10, 9, 254),
                    });
                }

                if (button(BUTTON_QUIT_ID, LAYLA_TEXT_SLICE("Quit"))) quit = true;

                if (tooltip_open) {
                    Layla_ContainerID(TOOLTIP_ID,
                        .style = {
                            .background = LAYLA_COLOR(30, 30, 30),
                            .padding = {.left = 1, .right = 1},
                            .border = {.width = 1, .color = LAYLA_RGB(255, 255, 255)},
                            .size = {.w = LAYLA_FIT(), .h = LAYLA_FIT()},
                        },
                        .floating = {
                            .attach_to = {
                                .type = LAYLA_ATTACH_TO_ELEMENT,
                                .as.element.id = BUTTON_QUIT_ID,
                            },
                            .attach_point = {
                                .parent = {.x = LAYLA_ALIGN_CENTER, .y = LAYLA_ALIGN_START},
                                .element = {.x = LAYLA_ALIGN_CENTER, .y = LAYLA_ALIGN_END},
                            },
                            .z_index = 10,
                        },
                    ) {
                        Layla_Text(
                            .text = LAYLA_TEXT_SLICE("Right-click tooltip"),
                            .style.color = LAYLA_RGB(255, 255, 255),
                        );
                    }
                }

                Layla_Container(.style = {
                    .size = {.w = LAYLA_FILL(), .h = LAYLA_FIXED(5)},
                    .background = LAYLA_COLOR(195, 255, 57),
                    .align_self = LAYLA_ALIGN_CENTER,
                });
            }

            Layla_CommandSlice cmds = layla_layout_end();

            for (isize i = 0; i < cmds.count; ++i) {
                Layla_Command cmd = cmds.items[i];
                switch (cmd.type) {
                    case LAYLA_CMD_RECTANGLE: {
                        Layla_CommandRectangle rectangle = cmd.as.rectangle;
                        brenda_rectangle_fill(*(Brenda_Rectangle *)&rectangle, *(Brenda_RGB *)&rectangle.color);
                        break;
                    }

                    case LAYLA_CMD_TEXT: {
                        Layla_CommandText text = cmd.as.text;
                        Brenda_TextEffect effect = { .color = *(Brenda_RGB *)&text.color, };
                        brenda_text_draw(text.x, text.y, text.slice.items, text.slice.count, effect);
                        break;
                    }

                    case LAYLA_CMD_BORDER: {
                        Layla_CommandBorder border = cmd.as.border;
                        Brenda_Rectangle rectangle = *(Brenda_Rectangle *)&border;
                        Brenda_TextEffect effect = { .color = *(Brenda_RGB *)&border.color, };
                        brenda_box_draw(rectangle, effect);
                        break;
                    }

                    case LAYLA_CMD_CUSTOM: {
                        break;
                    }

                    case LAYLA_CMD_CLIP_START: {
                        Layla_CommandClipStart clip_start = cmd.as.clip_start;
                        brenda_clip_push_rectangle(*(Brenda_Rectangle *)&clip_start);
                        break;
                    }

                    case LAYLA_CMD_CLIP_END: {
                        brenda_clip_pop();
                        break;
                    }

                    default: assert(false && "unknown cmd type");
                }
            }
        }
        brenda_frame_end();
    }

    return 0;
}
