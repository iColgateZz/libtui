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
        .type = LAYOUT_NODE_CONTAINER,
        // .container.style.size.w.mode = SIZE_FIXED,
        // .container.style.size.w.fixed.value = 5,
        // .container.style.size.h.mode = SIZE_FIXED,
        // .container.style.size.h.fixed.value = 5,
        .container.style.color = (Color) {127, 9, 254},
    };

    LayoutNode c1 = {
        .type = LAYOUT_NODE_CONTAINER,
        .container.style.size.w.mode = SIZE_FIXED,
        .container.style.size.w.fixed.value = 5,
        .container.style.size.h.mode = SIZE_FIXED,
        .container.style.size.h.fixed.value = 5,
        .container.style.color = (Color) {10, 9, 254},
        .parent = &root,
    };

    LayoutNode c2 = {
        .type = LAYOUT_NODE_CONTAINER,
        .container.style.size.w.mode = SIZE_FIXED,
        .container.style.size.w.fixed.value = 20,
        .container.style.size.h.mode = SIZE_FIXED,
        .container.style.size.h.fixed.value = 3,
        .container.style.color = (Color) {10, 9, 8},
        .parent = &root,
    };

    list_append(&root.container.children, c1);
    list_append(&root.container.children, c2);

    while (!is_codepoint(cp("q"))) {
        begin_frame();
        {
            layout(&root);
            
            LayoutCommand cmd;
            while (layout_cmd_next(&cmd)) {
                switch (cmd.type) {
                    case LAYOUT_CMD_CLIP_START: clip_push_rect(*(Rectangle *)&cmd.clip); break;
                    case LAYOUT_CMD_CLIP_END: clip_pop(); break;
                    case LAYOUT_CMD_RECT: {
                        fill_box(
                            *(Rectangle *)&cmd.rect,
                            (Effect) {
                                .bg = *(RGB *)&cmd.rect.color,
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
