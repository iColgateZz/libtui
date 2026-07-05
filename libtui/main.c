#define PSH_CORE_IMPL
    #include "psh_core.h"
#undef PSH_CORE_IMPL

#include "layla.h"
#define LIBTUI_RENDERER_IMPL
    #include "renderer.h"

void update_layout_input(Event event) {
    if (!event_is_mouse(event)) return;

    layla_cursor_set_position(event.as.mouse.x, event.as.mouse.y);

    if (event_is(event, EScrollUp)) {
        layla_scroll_update(-1);
    } else if (event_is(event, EScrollDown)) {
        layla_scroll_update(1);
    }
}

i32 main(void) {
    init_terminal();
    set_fps(60);

    b32 quit = false;
    while (!quit) {
        begin_frame();

        Slice(Event) events = get_events();
        for (isize i = 0; i < events.count; ++i) {
            Event event = events.items[i];
            if (event_is_codepoint(event, cp("q"))) quit = true;
            update_layout_input(event);
        }

        {
            u32 w = get_terminal_width();
            u32 h = get_terminal_height();
            layla_screen_set_dimensions(w, h);
            layla_begin();

            Layla_Container(1, .style = {
                .color = {196, 240, 120},
                .direction = LAYLA_DIR_ROW,
                .size = {.w = LAYLA_FIXED(w), .h = LAYLA_FIXED(h)},
            }) {
                Layla_Container(2, .style = {
                    .size = {.w = LAYLA_FILL(0, 10), .h = LAYLA_FILL(0, INT32_MAX)},
                    .color = {255, 133, 182},
                    .align_self = LAYLA_ALIGN_CENTER,
                });

                Layla_Container(3, .style = {
                    .size = {.w = LAYLA_FILL(0, INT32_MAX), .h = LAYLA_FIXED(1)},
                    .color = {233, 255, 57},
                    .align_self = LAYLA_ALIGN_CENTER,
                    .direction = LAYLA_DIR_COL,
                    .padding = 1,
                    .scroll = LAYLA_SCROLL_Y,
                }) {
                    Layla_Text(4, .text = LAYLA_TEXT_SLICE("LibTUI text wraps inside containers. LibTUI text wraps inside containers."),
                        .style = {
                            .color = {255, 255, 255},
                            .alignment = LAYLA_ALIGN_CENTER,
                        },
                    );
                    Layla_Container(5, .style = {
                        .size = {.w = LAYLA_FILL(0, INT32_MAX), .h = LAYLA_FIXED(5)},
                        .color = {10, 9, 254},
                    });
                }

                Layla_Container(6, .style = {
                    .size = {.w = LAYLA_FILL(0, INT32_MAX), .h = LAYLA_FIXED(5)},
                    .color = {195, 255, 57},
                    .align_self = LAYLA_ALIGN_CENTER,
                });
            }

            Layla_CommandSlice cmds = layla_end();

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
                        byte *start = text.text_slice.items;
                        byte *end = start + text.text_slice.count;

                        while (start < end) {
                            CodePoint cp = utf8_next(&start, end);
                            put_cp(x, text.y, cp);
                            merge_effect(x, text.y, effect);
                            x += cp.display_width;
                        }

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
