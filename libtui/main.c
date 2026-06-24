#define PSH_CORE_IMPL
#define PSH_NO_ECHO
#define PSH_CORE_NO_PREFIX
    #include "psh_core/psh_core.h"
#undef PSH_CORE_IMPL

#define LIBTUI_LAYOUT_IMPL
    #include "layout.h"
#define LIBTUI_RENDERER_IMPL
    #include "renderer.h"

void push_renderer_event_to_layout(Event renderer_event) {
    Layout_Event event = {0};

    if (event_is(renderer_event, EMouseLeft)) {
        event = (Layout_Event) {
            .type = LAYOUT_EVENT_MOUSE_LEFT,
            .mouse = {renderer_event.mouse.x, renderer_event.mouse.y, renderer_event.mouse.pressed},
        };
    } else if (event_is(renderer_event, EMouseRight)) {
        event = (Layout_Event) {
            .type = LAYOUT_EVENT_MOUSE_RIGHT,
            .mouse = {renderer_event.mouse.x, renderer_event.mouse.y, renderer_event.mouse.pressed},
        };
    } else if (event_is(renderer_event, EMouseMiddle)) {
        event = (Layout_Event) {
            .type = LAYOUT_EVENT_MOUSE_MIDDLE,
            .mouse = {renderer_event.mouse.x, renderer_event.mouse.y, renderer_event.mouse.pressed},
        };
    } else if (event_is(renderer_event, EScrollUp)) {
        event = (Layout_Event) {
            .type = LAYOUT_EVENT_SCROLL_UP,
            .mouse = {renderer_event.mouse.x, renderer_event.mouse.y, renderer_event.mouse.pressed},
        };
    } else if (event_is(renderer_event, EScrollDown)) {
        event = (Layout_Event) {
            .type = LAYOUT_EVENT_SCROLL_DOWN,
            .mouse = {renderer_event.mouse.x, renderer_event.mouse.y, renderer_event.mouse.pressed},
        };
    } else if (event_is(renderer_event, EMouseDrag)) {
        event = (Layout_Event) {
            .type = LAYOUT_EVENT_MOUSE_DRAG,
            .mouse = {renderer_event.mouse.x, renderer_event.mouse.y, renderer_event.mouse.pressed},
        };
    } else if (event_is(renderer_event, ETermKey)) {
        event = (Layout_Event) {
            .type = LAYOUT_EVENT_KEY,
            .key = renderer_event.term_key,
        };
    } else if (event_is(renderer_event, ECodePoint)) {
        event = (Layout_Event) {
            .type = LAYOUT_EVENT_TEXT,
            .text = renderer_event.codepoint,
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

    List(CodePoint) cps = {0};
    s8 text = s8("LibTUI text wraps inside containers. LibTUI text wraps inside containers.");
    byte *start = text.s;
    byte *end = text.s + text.len;
    while (start < end) {
        list_append(&cps, utf8_next(&start, end));
    }
    Slice(CodePoint) slice = {
        .items = cps.items,
        .count = cps.count,
    };

    b32 quit = false;
    while (!quit) {
        begin_frame();

        Slice(Event) events = get_events();
        for (isize i = 0; i < events.count; ++i) {
            Event event = events.items[i];
            if (event_is_codepoint(event, cp("q"))) quit = true;
            push_renderer_event_to_layout(event);
        }

        {
            u32 w = get_terminal_width();
            u32 h = get_terminal_height();
            layout_begin(w, h);

            Container(1, .style = {
                .color = {127, 9, 254},
                .direction = {DIR_ROW},
                .size = {.w = FIXED(w), .h = FIXED(h)},
            }) {
                Container(2, .style = {
                    .size = {.w = FILL(0, 10), .h = FILL(0, INT32_MAX)},
                    .color = {10, 100, 254},
                    .align_self = {ALIGN_CENTER},
                });

                Container(3, .style = {
                    .size = {.w = FILL(0, INT32_MAX), .h = FIXED(1)},
                    .color = {10, 250, 8},
                    .align_self = {ALIGN_CENTER},
                    .direction = {DIR_COL},
                    .padding = 1,
                    .scroll = SCROLL_Y,
                }) {
                    Text(4, .text = slice,
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
                    .size = {.w = FILL(0, 10), .h = FIXED(5)},
                    .color = {10, 100, 254},
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
                        for (isize j = 0; j < text.text.count; ++j) {
                            CodePoint cp = text.text.items[j];
                            put_cp(x, text.y, cp, effect);
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
