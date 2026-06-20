#define PSH_CORE_IMPL
#define PSH_NO_ECHO
#define PSH_CORE_NO_PREFIX
    #include "psh_core/psh_core.h"
#undef PSH_CORE_IMPL

#define LIBTUI_LAYOUT_IMPL
    #include "layout.h"
#define LIBTUI_RENDERER_IMPL
    #include "renderer.h"

i32 main(i32 argc, byte *argv[]) {
    REBUILD_UNITY_AUTO(argc, argv);

    init_terminal();
    set_max_timeout_ms(100);

    List(CodePoint) cps = {0};
    s8 text = s8("LibTUI text wraps inside containers. LibTUI text wraps inside containers.");
    byte *start = text.s;
    byte *end = text.s + text.len;
    while (start < end) {
        list_append(&cps, utf8_next(&start, end));
    }

    while (!is_codepoint(cp("q"))) {
        begin_frame();
        {
            u32 w = get_terminal_width();
            u32 h = get_terminal_height();
            layout_begin(w, h);

            Container(.style = {
                .color = {127, 9, 254},
                .direction = {DIR_ROW},
                .size = {.w = FIXED(w), .h = FIXED(h)},
            }) {
                Container(.style = {
                    .size = {.w = FILL(0, 10), .h = FILL(0, INT32_MAX)},
                    .color = {10, 100, 254},
                    .align_self = {ALIGN_CENTER},
                });

                Container(.style = {
                    .size = {.w = FILL(0, INT32_MAX), .h = FIXED(10)},
                    .color = {10, 250, 8},
                    .align_self = {ALIGN_CENTER},
                    .direction = {DIR_COL},
                    .padding = 1,
                }) {
                    Text(.text = cps,
                        .style = {
                            .color = {255, 255, 255}
                        },
                    );
                    Container(.style = {
                        .size = {.w = FILL(0, INT32_MAX), .h = FIXED(5)},
                        .color = {10, 9, 254},
                    });
                }

                Container(.style = {
                    .size = {.w = FILL(0, 10), .h = FIXED(5)},
                    .color = {10, 100, 254},
                    .align_self = {ALIGN_CENTER},
                });
            }

            List(LayoutCommand) cmds = layout_end();

            for (usize i = 0; i < cmds.count; ++i) {
                LayoutCommand cmd = cmds.items[i];
                // debug_cmd(0, i, cmd);
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
                        for (usize j = 0; j < text.text.count; ++j) {
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
