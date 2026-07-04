#define PSH_CORE_IMPL
    #include "psh_core.h"
#undef PSH_CORE_IMPL

#include "layla.h"
#define LIBTUI_RENDERER_IMPL
    #include "renderer.h"

void update_layout_input(Event event) {
    if (!event_is_mouse(event)) return;

    layout_cursor_set_position(event.mouse.x, event.mouse.y);

    if (event_is(event, EScrollUp)) {
        layout_scroll_update(-1);
    } else if (event_is(event, EScrollDown)) {
        layout_scroll_update(1);
    }
}

i32 main() {
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
            layout_screen_set_dimensions(w, h);
            layout_begin();

            Container(1, .style = {
                .color = {196, 240, 120},
                .direction = {DIR_ROW},
                .size = {.w = FIXED(w), .h = FIXED(h)},
            }) {
                Container(2, .style = {
                    .size = {.w = FILL(0, 10), .h = FILL(0, INT32_MAX)},
                    .color = {255, 133, 182},
                    .align_self = {ALIGN_CENTER},
                });

                Container(3, .style = {
                    .size = {.w = FILL(0, INT32_MAX), .h = FIXED(1)},
                    .color = {233, 255, 57},
                    .align_self = {ALIGN_CENTER},
                    .direction = {DIR_COL},
                    .padding = 1,
                    .scroll = SCROLL_Y,
                }) {
                    Text(4, .text = LAYOUT_TEXT("LibTUI text wraps inside containers. LibTUI text wraps inside containers."),
                        .style = {
                            .color = {255, 255, 255}
                        },
                    );
                    Container(5, .style = {
                        .size = {.w = FILL(0, INT32_MAX), .h = FIXED(5)},
                        .color = {10, 9, 254},
                    });
                }

                Container(6, .style = {
                    .size = {.w = FILL(0, INT32_MAX), .h = FIXED(5)},
                    .color = {195, 255, 57},
                    .align_self = {ALIGN_CENTER},
                });
            }

            Layout_CommandSlice cmds = layout_end();

            for (isize i = 0; i < cmds.count; ++i) {
                Layout_Command cmd = cmds.items[i];
                switch (cmd.type) {
                    case LAYOUT_CMD_RECT: {
                        Rectangle rect = {
                            .x = cmd._LAYOUT_CMD_RECT.x,
                            .y = cmd._LAYOUT_CMD_RECT.y,
                            .w = cmd._LAYOUT_CMD_RECT.w,
                            .h = cmd._LAYOUT_CMD_RECT.h,
                        };
                        fill_box(
                            rect,
                            (Effect) {
                                .bg = {
                                    cmd._LAYOUT_CMD_RECT.color.r,
                                    cmd._LAYOUT_CMD_RECT.color.g,
                                    cmd._LAYOUT_CMD_RECT.color.b,
                                },
                                .flags = EFFECT_BG,
                            }
                        ); break;
                    }
                    case LAYOUT_CMD_TEXT: {
                        i32 x = cmd._LAYOUT_CMD_TEXT.x;
                        Effect effect = {
                            .fg = {
                                cmd._LAYOUT_CMD_TEXT.style.color.r,
                                cmd._LAYOUT_CMD_TEXT.style.color.g,
                                cmd._LAYOUT_CMD_TEXT.style.color.b,
                            },
                            .flags = EFFECT_FG,
                        };
                        byte *start = cmd._LAYOUT_CMD_TEXT.text.items;
                        byte *end = start + cmd._LAYOUT_CMD_TEXT.text.count;
                        while (start < end) {
                            CodePoint cp = utf8_next(&start, end);
                            put_cp(x, cmd._LAYOUT_CMD_TEXT.y, cp);
                            merge_effect(x, cmd._LAYOUT_CMD_TEXT.y, effect);
                            x += cp.display_width;
                        }
                        break;
                    }
                    case LAYOUT_CMD_CLIP_START: {
                        Rectangle clip = {
                            .x = cmd._LAYOUT_CMD_CLIP_START.x,
                            .y = cmd._LAYOUT_CMD_CLIP_START.y,
                            .w = cmd._LAYOUT_CMD_CLIP_START.w,
                            .h = cmd._LAYOUT_CMD_CLIP_START.h,
                        };
                        clip_push_rect(clip);
                        break;
                    }
                    case LAYOUT_CMD_CLIP_END: clip_pop(); break;
                    default: assert(false && "unknown cmd type");
                }
            }
        }
        end_frame();
    }

    return 0;
}
