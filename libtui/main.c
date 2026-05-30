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
        ._LAYOUT_NODE_CONTAINER.style.color = (Color) {127, 9, 254},
    };

    LayoutNode c1 = {
        .type = LAYOUT_NODE_CONTAINER,
        ._LAYOUT_NODE_CONTAINER.style.size.w.mode = SIZE_FIXED,
        ._LAYOUT_NODE_CONTAINER.style.size.w.fixed.value = 5,
        ._LAYOUT_NODE_CONTAINER.style.size.h.mode = SIZE_FIXED,
        ._LAYOUT_NODE_CONTAINER.style.size.h.fixed.value = 5,
        ._LAYOUT_NODE_CONTAINER.style.color = (Color) {10, 9, 254},
        .parent = &root,
    };

    LayoutNode c2 = {
        .type = LAYOUT_NODE_CONTAINER,
        ._LAYOUT_NODE_CONTAINER.style.size.w.mode = SIZE_FIXED,
        ._LAYOUT_NODE_CONTAINER.style.size.w.fixed.value = 20,
        ._LAYOUT_NODE_CONTAINER.style.size.h.mode = SIZE_FIXED,
        ._LAYOUT_NODE_CONTAINER.style.size.h.fixed.value = 3,
        ._LAYOUT_NODE_CONTAINER.style.color = (Color) {10, 9, 8},
        .parent = &root,
    };

    list_append(&root._LAYOUT_NODE_CONTAINER.children, c1);
    list_append(&root._LAYOUT_NODE_CONTAINER.children, c2);

    while (!is_codepoint(cp("q"))) {
        begin_frame();
        {
            layout(&root);
            
            LayoutCommand cmd;
            while (layout_cmd_next(&cmd)) {
                match(cmd) {
                    case(LAYOUT_CMD_RECT, rect) {
                        fill_box(
                            *(Rectangle *)&rect,
                            (Effect) {
                                .bg = *(RGB *)&rect.color,
                                .flags = EFFECT_BG,
                            }
                        );
                    }
                    case(LAYOUT_CMD_CLIP_START, clip) clip_push_rect(*(Rectangle *)&clip);
                    case(LAYOUT_CMD_CLIP_END) clip_pop();
                    otherwise assert(false && "unknown cmd type");
                }
            }
        }
        end_frame();
    }

    return 0;
}
