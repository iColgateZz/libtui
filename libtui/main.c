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
                    .size = {.w = FILL(0, INT32_MAX), .h = FIXED(5)},
                    .color = {10, 9, 254},
                    .align_self = {ALIGN_CENTER},
                });

                Container(.style = {
                    .size = {.w = FILL(0, INT32_MAX), .h = FIXED(10)},
                    .color = {10, 250, 8},
                    .align_self = {ALIGN_CENTER},
                });
            }

            List(LayoutCommand) cmds = layout_end();

            for (usize i = 0; i < cmds.count; ++i) {
                LayoutCommand cmd = cmds.items[i];
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
