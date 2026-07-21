#define PSH_CORE_IMPL
    #include "psh_core.h"
#undef PSH_CORE_IMPL

#include "layla.h"
#define LIBTUI_RENDERER_IMPL
    #include "renderer.h"

enum {
    BUTTON_QUIT_ID = 1,
};

static i32 text_measure(Layla_TextSlice text, void *userdata) {
    UNUSED(userdata);

    i32 width = 0;
    byte *cursor = text.items;
    byte *end = text.items + text.count;
    while (cursor < end) {
        CodePoint codepoint = utf8_next(&cursor, end);
        width += codepoint.display_width;
    }

    return width;
}

static b32 button(Layla_ElementID id, Layla_TextSlice label, b32 left_mouse_pressed) {
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

    return hovered && left_mouse_pressed;
}

void update_layout_input(Event event) {
    if (!event_is_mouse(event)) return;

    layla_state_set_cursor_position(event.as.mouse.x, event.as.mouse.y);

    if (event_is(event, EScrollUp)) {
        layla_scroll_offset_update_on_hovered_element(-1);
    } else if (event_is(event, EScrollDown)) {
        layla_scroll_offset_update_on_hovered_element(1);
    }
}

i32 main(void) {
    layla_state_set_text_measure_function(text_measure, NULL);
    init_terminal();
    set_fps(60);

    b32 quit = false;
    while (!quit) {
        begin_frame();

        b32 left_mouse_pressed = false;
        Slice(Event) events = get_events();
        for (isize i = 0; i < events.count; ++i) {
            Event event = events.items[i];
            if (event_is_codepoint(event, cp("q"))) quit = true;
            update_layout_input(event);
            if (event_is(event, EMouseLeft) && event.as.mouse.pressed) left_mouse_pressed = true;
        }

        {
            u32 w = get_terminal_width();
            u32 h = get_terminal_height();
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

                Layla_Container(.style = {
                    .size = {.w = LAYLA_PERCENT(.8), .h = LAYLA_FIT()},
                    .background = LAYLA_COLOR(233, 255, 57),
                    .align_self = LAYLA_ALIGN_CENTER,
                    .direction = LAYLA_DIR_COL,
                    .padding = {.left = 1, .right = 1, .top = 5, .bottom = 1},
                    .border = {.width = 1},
                    .scroll = {
                        .id = 1,
                        .axis = LAYLA_SCROLL_Y,
                    },
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

                    Layla_Container(
                        .style = {
                            .background = LAYLA_COLOR(0, 0, 0),
                            .size = {.w = LAYLA_FIXED(10), .h = LAYLA_FIXED(10)}
                        },
                        .floating = {
                            .attach_to = LAYLA_ATTACH_TO_PARENT,
                            .z_index = 1,
                            .align_x = LAYLA_ALIGN_CENTER,
                        },
                    );
                }

                if (button(BUTTON_QUIT_ID, LAYLA_TEXT_SLICE("Quit"), left_mouse_pressed)) quit = true;

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
                        fill_box(
                            *(Rectangle *)&rectangle,
                            (Effect) { .bg = *(RGB *)&rectangle.color, .flags = EFFECT_BG }
                        );

                        break;
                    }

                    case LAYLA_CMD_TEXT: {
                        Layla_CommandText text = cmd.as.text;

                        Effect effect = {
                            .fg = *(RGB *)&text.color,
                            .flags = EFFECT_FG,
                        };

                        i32 x = text.x;
                        byte *start = text.slice.items;
                        byte *end = start + text.slice.count;

                        while (start < end) {
                            CodePoint cp = utf8_next(&start, end);
                            put_cp(x, text.y, cp);
                            merge_effect(x, text.y, effect);
                            x += cp.display_width;
                        }

                        break;
                    }

                    case LAYLA_CMD_BORDER: {
                        Layla_CommandBorder border = cmd.as.border;
                        Rectangle rectangle = *(Rectangle *)&border;
                        Effect effect = {
                            .fg = *(RGB *)&border.color,
                            .flags = EFFECT_FG,
                        };

                        if (border.userdata == NULL) {
                            i32 x0 = rectangle.x;
                            i32 y0 = rectangle.y;
                            i32 x1 = rectangle.x + rectangle.w - 1;
                            i32 y1 = rectangle.y + rectangle.h - 1;

                            draw_box(rectangle);

                            for (i32 x = x0; x <= x1; ++x) {
                                merge_effect(x, y0, effect);
                                merge_effect(x, y1, effect);
                            }
                            for (i32 y = y0; y <= y1; ++y) {
                                merge_effect(x0, y, effect);
                                merge_effect(x1, y, effect);
                            }
                        }

                        break;
                    }

                    case LAYLA_CMD_CUSTOM: {
                        break;
                    }

                    case LAYLA_CMD_CLIP_START: {
                        Layla_CommandClipStart clip_start = cmd.as.clip_start;
                        clip_push_rect(*(Rectangle *)&clip_start);
                        break;
                    }

                    case LAYLA_CMD_CLIP_END: {
                        clip_pop();
                        break;
                    }

                    default: assert(false && "unknown cmd type");
                }
            }
        }
        end_frame();
    }

    return 0;
}
