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

    LayoutNode root = {
        .type = LAYOUT_NODE_RECT,
        .style.size.w.value = 5,
        .style.size.h.value = 5,
        .style.color = (Color) {127, 9, 254},
    };

    while (!is_codepoint(cp("q"))) {
        begin_frame();
        {
            layout_nodes(&root);

            LayoutCommand cmd;
            while (layout_cmd_next(&cmd)) {
                switch (cmd.type) {
                    case LAYOUT_CMD_CLIP_START: clip_push(cmd.clip.x, cmd.clip.y, cmd.clip.w, cmd.clip.h); break;
                    case LAYOUT_CMD_CLIP_END: clip_pop(); break;
                    case LAYOUT_CMD_RECT: {
                        fill_box(
                            (Rectangle) {cmd.rect.x, cmd.rect.y, cmd.rect.w, cmd.rect.h},
                            (Effect) {
                                .bg = (RGB) {cmd.rect.color.r, cmd.rect.color.g, cmd.rect.color.b},
                                .flags = EFFECT_BG,
                            }
                        );
                    } break;
                    default: assert(false && "unknown cmd type");
                }
            }
        }
        end_frame();
    }

    return 0;
}
