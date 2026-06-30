#define PSH_CORE_IMPL
#define PSH_NO_ECHO
#define PSH_CORE_NO_PREFIX
    #include "psh_core/psh_core.h"
#undef PSH_CORE_IMPL

#define LIBTUI_LAYOUT_IMPL
    #include "layout.h"
#define LIBTUI_RENDERER_IMPL
    #include "renderer.h"

void push_renderer_event_to_layout(Event *renderer_event) {
    Layout_Event event = {0};

    if (event_is(*renderer_event, EMouseLeft)) {
        event = (Layout_Event) {
            .type = LAYOUT_EVENT_MOUSE_LEFT,
            .mouse = {renderer_event->mouse.x, renderer_event->mouse.y, renderer_event->mouse.pressed},
        };
    } else if (event_is(*renderer_event, EMouseRight)) {
        event = (Layout_Event) {
            .type = LAYOUT_EVENT_MOUSE_RIGHT,
            .mouse = {renderer_event->mouse.x, renderer_event->mouse.y, renderer_event->mouse.pressed},
        };
    } else if (event_is(*renderer_event, EMouseMiddle)) {
        event = (Layout_Event) {
            .type = LAYOUT_EVENT_MOUSE_MIDDLE,
            .mouse = {renderer_event->mouse.x, renderer_event->mouse.y, renderer_event->mouse.pressed},
        };
    } else if (event_is(*renderer_event, EScrollUp)) {
        event = (Layout_Event) {
            .type = LAYOUT_EVENT_SCROLL_UP,
            .mouse = {renderer_event->mouse.x, renderer_event->mouse.y, renderer_event->mouse.pressed},
        };
    } else if (event_is(*renderer_event, EScrollDown)) {
        event = (Layout_Event) {
            .type = LAYOUT_EVENT_SCROLL_DOWN,
            .mouse = {renderer_event->mouse.x, renderer_event->mouse.y, renderer_event->mouse.pressed},
        };
    } else if (event_is(*renderer_event, EMouseDrag)) {
        event = (Layout_Event) {
            .type = LAYOUT_EVENT_MOUSE_DRAG,
            .mouse = {renderer_event->mouse.x, renderer_event->mouse.y, renderer_event->mouse.pressed},
        };
    } else if (event_is(*renderer_event, ETermKey)) {
        event = (Layout_Event) {
            .type = LAYOUT_EVENT_KEY,
            .key = renderer_event->term_key,
        };
    } else if (event_is(*renderer_event, ECodePoint)) {
        event = (Layout_Event) {
            .type = LAYOUT_EVENT_TEXT,
            .text = {
                .items = renderer_event->codepoint.raw,
                .count = renderer_event->codepoint.raw_len,
            },
        };
    } else {
        return;
    }

    layout_event_push(event);
}

i32 main(i32 argc, byte *argv[]) {
    REBUILD_UNITY_AUTO(argc, argv);

    init_terminal();
    set_fps(60);

    b32 quit = false;
    while (!quit) {
        begin_frame();

        Slice(Event) events = get_events();
        for (isize i = 0; i < events.count; ++i) {
            Event event = events.items[i];
            if (event_is_codepoint(event, cp("q"))) quit = true;
            push_renderer_event_to_layout(&events.items[i]);
        }

        {
            u32 w = get_terminal_width();
            u32 h = get_terminal_height();
            layout_begin(w, h);

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

            Slice(Layout_Command) cmds = layout_end();

            for (isize i = 0; i < cmds.count; ++i) {
                Layout_Command cmd = cmds.items[i];
                match(cmd) {
                    case(LAYOUT_CMD_RECT, rect) {
                        fill_box(
                            *(Rectangle *)&rect,
                            (Effect) {
                                .bg = *(RGB *)&rect.color,
                                .flags = EFFECT_BG,
                            }
                        ); break;
                    }
                    case(LAYOUT_CMD_TEXT, text) {
                        i32 x = text.x;
                        Effect effect = {
                            .fg = *(RGB *)&text.style.color,
                            .flags = EFFECT_FG,
                        };
                        byte *start = text.text.items;
                        byte *end = text.text.items + text.text.count;
                        while (start < end) {
                            CodePoint cp = utf8_next(&start, end);
                            put_cp(x, text.y, cp);
                            merge_effect(x, text.y, effect);
                            x += cp.display_width;
                        }
                        break;
                    }
                    case(LAYOUT_CMD_CLIP_START, clip) clip_push_rect(*(Rectangle *)&clip); break;
                    case(LAYOUT_CMD_CLIP_END) clip_pop(); break;
                    otherwise assert(false && "unknown cmd type");
                }
            }
        }
        end_frame();
    }

    return 0;
}
