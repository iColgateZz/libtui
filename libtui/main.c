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

    //TODO: Retained frontend widgets should have stable IDs
    //      that can be used by the layout nodes for state referencing?

    LayoutNode root = {
        .type = LAYOUT_NODE_CONTAINER,
        ._LAYOUT_NODE_CONTAINER.style.color = {127, 9, 254},
        .parent = -1,
    };
    LayoutNodeID rootID = layout_node_push(root);
    LayoutNode *rootp = layout_node_get(rootID);

    {
        LayoutNode c1 = {
            .type = LAYOUT_NODE_CONTAINER,
            ._LAYOUT_NODE_CONTAINER.style.size = {.w = FIXED(5), .h = FIXED(5)},
            ._LAYOUT_NODE_CONTAINER.style.color = {10, 9, 254},
            .parent = rootID,
        };
        LayoutNodeID c1ID = layout_node_push(c1);
        list_append(&rootp->_LAYOUT_NODE_CONTAINER.children, c1ID);
    
        LayoutNode c2 = {
            .type = LAYOUT_NODE_CONTAINER,
            ._LAYOUT_NODE_CONTAINER.style.size = {.w = FIXED(20), .h = FIXED(3)},
            ._LAYOUT_NODE_CONTAINER.style.color = {10, 9, 8},
            .parent = rootID,
        };
        LayoutNodeID c2ID = layout_node_push(c2);
        list_append(&rootp->_LAYOUT_NODE_CONTAINER.children, c2ID);
    }

    while (!is_codepoint(cp("q"))) {
        begin_frame();
        {
            layout(rootID);

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
