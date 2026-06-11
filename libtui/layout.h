#ifndef LIBTUI_LAYOUT_INCLUDE
#define LIBTUI_LAYOUT_INCLUDE

#define PSH_CORE_NO_PREFIX
#include "psh_core/psh_core.h"
#include "unicode.h"

//TODO: move to psh_core
#define typeof __typeof__
#define match(tu) for (typeof(tu) _tu = tu, *p = &_tu; p; p = NULL) switch ((tu).type)
// may be fixed with __VA_OPT(__VA_ARGS__,) in C23 or
// https://medium.com/@pauljlucas/using-advanced-c-preprocessor-macros-for-a-pre-c23-c-20-va-opt-substitute-bccefde27817
#define case(...)   xcase(__VA_ARGS__, _case2, _case1)(__VA_ARGS__)
#define xcase(a, b, c, ...) c
#define _case2(tag, varname) case (tag): ;typeof(_tu._##tag) varname = _tu._##tag;
#define _case1(tag) case (tag): ;
#define otherwise default:
#define unwrap_into(tu, tag, varname) typeof((tu)._##tag) varname = (tu)._##tag;
#define matches(tu, tag) (tu).type == tag

#define tag(T, tag, ...) ((T) {.type = tag, ._##tag = __VA_ARGS__})
#define tag0(T, tag) ((T) {.type = tag})

#define packed_enum enum __attribute__((__packed__))

typedef struct {
    packed_enum {
        SIZE_FIT,
        SIZE_FILL,
        SIZE_FIXED,
    } type;
    union {
        struct {
            i32 value;
        } _SIZE_FIXED;
        struct {
            i32 min;
            i32 max;
        } _SIZE_FIT;
        struct {
            i32 min;
            i32 max;
        } _SIZE_FILL;
    };
} Size;

#define FIXED(value) tag(Size, SIZE_FIXED, {value})

typedef struct {
    Size w;
    Size h;
} Sizing;

typedef struct {
    u8 r, g, b;
} Color;

typedef struct {
    packed_enum {
        DIR_ROW,
        DIR_COL,
    } type;
} Direction;

typedef struct {
    packed_enum {
        ALIGN_START,
        ALIGN_CENTER,
        ALIGN_END,
    } type;
} Alignment;

typedef struct {
    Sizing size;
    Color color;
    u8 padding;
    u8 spacing;
    // u8 margin;
    // BorderStyle border;
    Direction direction;
    Alignment align_children;
    Alignment align_self;
} LayoutNodeStyle;

typedef struct {
    LayoutNodeStyle style;
    // void *userdata;
} ContainerConfig;

typedef struct {

} TextStyle;

typedef i32 LayoutNodeID;
list_def(LayoutNodeID);

typedef struct {
    i32 offset;
    u32 count;
} ChildIdxSlice;

typedef struct {
    LayoutNodeID parent;
    i32 x, y; // resolved coords
    i32 w, h; // resolved w, h
    ChildIdxSlice children;
    
    packed_enum {
        //TODO: for custom nodes there may be 2 variants:
        //      one that has a vtable with all pipeline methods,
        //      one that only has a custom draw method that emits renderer commands.

        // Maybe instead of adding a new type just emit a custom command that gives
        // user enough context so that they can adapt it for the renderer?
        LAYOUT_NODE_CONTAINER,
        LAYOUT_NODE_TEXT,
    } type;
    union {
        struct {
            ContainerConfig config;
        } _LAYOUT_NODE_CONTAINER;
        struct {
            List(CodePoint) text;
            TextStyle style;
        } _LAYOUT_NODE_TEXT;
    };
} LayoutNode;

list_def(LayoutNode);

typedef struct {
    packed_enum {
        LAYOUT_CMD_RECT,
        // LAYOUT_CMD_TEXT,
        LAYOUT_CMD_CLIP_START,
        LAYOUT_CMD_CLIP_END,
        // LAYOUT_CMD_BORDER, // pass concrete codepoints to draw
    } type;
    union {
        struct {
            i32 x, y, w, h;
            Color color;
        } _LAYOUT_CMD_RECT;
        struct {
            i32 x, y, w, h;
        } _LAYOUT_CMD_CLIP_START;
    };
} LayoutCommand;

list_def(LayoutCommand);

void layout_begin(i32 w, i32 h);
List(LayoutCommand) layout_end();
void layout_open(ContainerConfig conf);
void layout_close();
// hit testing, event handling, and state updates
// happen after layout phase

static u8 _latch = 0;

#define Container(...)  \
    for (_latch = (layout_open((ContainerConfig) {__VA_ARGS__}), 0); _latch < 1; _latch = 1, layout_close())

#endif

#ifdef LIBTUI_LAYOUT_IMPL

void layout(LayoutNodeID id);

static struct {
    List(LayoutNode) nodes;
    List(LayoutNodeID) open_stack;
    List(LayoutNodeID) children_temp;
    List(LayoutNodeID) children_persistent;
    List(LayoutCommand) cmds;
} Layout = {0};

LayoutNode *layout_node_by_id(LayoutNodeID id) {
    assert(0 <= id && (u32)id < Layout.nodes.count);
    return &Layout.nodes.items[id];
}

LayoutNode *layout_node_by_index(i32 index) {
    assert(0 <= index && (u32)index < Layout.children_persistent.count);
    LayoutNodeID id = Layout.children_persistent.items[index];
    return layout_node_by_id(id);
}

LayoutNodeID layout_node_push(LayoutNode node) {
    LayoutNodeID id = Layout.nodes.count;
    list_append(&Layout.nodes, node);
    return id;
}

void layout_begin(i32 w, i32 h) {
    // reset state
    Layout.nodes.count = 0;
    Layout.open_stack.count = 0;
    Layout.children_temp.count = 0;
    Layout.children_persistent.count = 0;
    Layout.cmds.count = 0;

    // open implicit root element
    layout_open((ContainerConfig) {
        .style = {
            .size = {.w = FIXED(w), .h = FIXED(h)},
            .direction = {DIR_COL},
        }
    });
}

List(LayoutCommand) layout_end() {
    // close implicit root element
    layout_close();

    // layout all elements
    #define ROOT_ID    0
    layout(ROOT_ID);

    // emit commands for renderer
    return Layout.cmds;
}

void layout_open(ContainerConfig conf) {
    LayoutNode node = tag(LayoutNode, LAYOUT_NODE_CONTAINER, {.config = conf});
    LayoutNodeID id = layout_node_push(node);
    list_append(&Layout.open_stack, id);
}

void layout_close() {
    LayoutNodeID closed_id = list_pop(&Layout.open_stack);
    LayoutNode *closed = layout_node_by_id(closed_id);
    closed->children.offset = Layout.children_persistent.count;
    // Take last IDs from the temp child stack
    // and attach them to contiguous child list
    Layout.children_temp.count -= closed->children.count;
    for (usize i = Layout.children_temp.count; i < closed->children.count; ++i) {
        LayoutNodeID child_id = Layout.children_temp.items[i];
        list_append(&Layout.children_persistent, child_id);
        // Attach parent to child
        LayoutNode *child = layout_node_by_id(child_id);
        child->parent = closed_id;
    }

    list_append(&Layout.children_temp, closed_id);
    if (closed_id > ROOT_ID) {
        LayoutNodeID parent_id = list_last(&Layout.open_stack);
        LayoutNode *parent = layout_node_by_id(parent_id);
        parent->children.count++;
    }
}

#define xmacro(namespace)               \
        X(namespace, intrinsic_width)   \
        X(namespace, fill_width)        \
        X(namespace, intrinsic_height)  \
        X(namespace, fill_height)       \
        X(namespace, positions)         \
        X(namespace, commands)

#define X(ns, fn)   void ns##_##fn(LayoutNodeID id);
xmacro(layout)
#undef X

void layout(LayoutNodeID id) {
    #define X(ns, fn)   ns##_##fn(id);
    xmacro(layout)
    #undef X
}

#define X(ns, fn)   void ns##_##fn(LayoutNode *node);
xmacro(container)
xmacro(text)
#undef X

#define X(ns, fn)                               \
void ns##_##fn(LayoutNodeID id) {               \
    LayoutNode *node = layout_node_by_id(id);     \
    match(*node) {                              \
        case(LAYOUT_NODE_TEXT)                  \
            text_##fn(node);                    \
            break;                              \
                                                \
        case(LAYOUT_NODE_CONTAINER)             \
            container_##fn(node);               \
            break;                              \
                                                \
        otherwise UNREACHABLE("Unknown type");  \
    }                                           \
}
xmacro(layout)
#undef X

//TODO: calculate minimal w/h for nodes during intrinsic steps so shrinking does not break children.
//TODO: deduplicate width, height, position logic.
void container_intrinsic_width(LayoutNode *node) {
    unwrap_into(*node, LAYOUT_NODE_CONTAINER, container);

    ChildIdxSlice children = node->children;
    for (usize i = 0; i < children.count; ++i) {
        LayoutNodeID child_id = Layout.children_persistent.items[children.offset + i];
        layout_intrinsic_width(child_id);
    }

    LayoutNodeStyle style = container.config.style;
    Size wsize = style.size.w;
    match(wsize) {
        case(SIZE_FIXED, fixed)
            //TODO: account for border
            //      do I add padding here?
            node->w = fixed.value;
            break;

        case(SIZE_FILL)
        case(SIZE_FIT, fit) {
            match(style.direction) {
                case(DIR_ROW) {
                    i32 children_width = 0;
                    for (usize i = 0; i < children.count; ++i) {
                        LayoutNode *child = layout_node_by_index(children.offset + i);
                        children_width += child->w;
                    }
                    node->w = children_width + 2 * style.padding + 
                            MAX(children.count - 1, 0) * style.spacing;
                    break;
                }

                case(DIR_COL) {
                    i32 max_child_w = 0;
                    for (usize i = 0; i < children.count; ++i) {
                        LayoutNode *child = layout_node_by_index(children.offset + i);
                        max_child_w = MAX(child->w, max_child_w);
                    }
                    node->w = max_child_w + 2 * style.padding;
                    break;
                }
            }

            if (fit.min < fit.max) 
                node->w = CLAMP(node->w, fit.min, fit.max);
        }
    }
}

void container_fill_width(LayoutNode *node) {
    unwrap_into(*node, LAYOUT_NODE_CONTAINER, container);

    LayoutNodeStyle style = container.config.style;
    ChildIdxSlice children = node->children;
    match(style.direction) {
        case(DIR_ROW) {
            i32 children_width = 0;
            for (usize i = 0; i < children.count; ++i) {
                LayoutNode *child = layout_node_by_index(children.offset + i);
                children_width += child->w;
            }

            i32 remaining_width = node->w - children_width - 2 * style.padding
                                  - MAX(children.count - 1, 0) * style.spacing;

            while (remaining_width > 0) {
                i32 smallest;
                i32 next_smallest;
                i32 smallest_count = 0;
                

            }

        }

        case(DIR_COL) {
            i32 content_width = node->w - 2 * style.padding;
            for (usize i = 0; i < children.count; ++i) {
                LayoutNode *child = layout_node_by_index(children.offset + i);
                
                match(*child) {
                    case(LAYOUT_NODE_TEXT) {
                        child->w = content_width;
                        break;
                    }
                    //TODO: clamp to min/max constraints
                    case(LAYOUT_NODE_CONTAINER, child_container) {
                        Size wsize = child_container.config.style.size.w;
                        if (matches(wsize, SIZE_FILL))
                            child->w = content_width;
                    }
                }
            }
        }
    }
}

void container_intrinsic_height(LayoutNode *node) {
    unwrap_into(*node, LAYOUT_NODE_CONTAINER, container);

    ChildIdxSlice children = node->children;
    for (usize i = 0; i < children.count; ++i) {
        LayoutNodeID child_id = Layout.children_persistent.items[children.offset + i];
        layout_intrinsic_height(child_id);
    }

    LayoutNodeStyle style = container.config.style;
    Size hsize = style.size.h;
    match(hsize) {
        case(SIZE_FIXED)
            //TODO: account for border
            //      do I add padding here?
            node->h = hsize._SIZE_FIXED.value;
            break;

        case(SIZE_FILL)
        case(SIZE_FIT, fit) {
            match(style.direction) {
                case(DIR_ROW) {
                    i32 max_child_h = 0;
                    for (usize i = 0; i < children.count; ++i) {
                        LayoutNode *child = layout_node_by_index(children.offset + i);
                        max_child_h = MAX(child->h, max_child_h);
                    }
                    
                    node->h = max_child_h + 2 * style.padding;
                    break;
                }
                case(DIR_COL) {
                    i32 children_height = 0;
                    for (usize i = 0; i < children.count; ++i) {
                        LayoutNode *child = layout_node_by_index(children.offset + i);
                        children_height += child->h;
                    }
                    node->h = children_height + 2 * style.padding + 
                            MAX(children.count - 1, 0) * style.spacing;
                }
            }

            if (fit.min < fit.max)
                node->h = CLAMP(node->h, fit.min, fit.max);
        }
    }
}

void container_fill_height(LayoutNode *node) { UNUSED(node); }

i32 align_cross(Alignment align, i32 parent_size, i32 parent_padding, i32 child_size);

void container_positions(LayoutNode *node) {
    if (node->parent < 0)
        node->x = node->y = 0;

    unwrap_into(*node, LAYOUT_NODE_CONTAINER, container);

    LayoutNodeStyle style = container.config.style;
    i32 pos_x = node->x + style.padding;
    i32 pos_y = node->y + style.padding;
    ChildIdxSlice children = node->children;

    match(style.direction) {
        case(DIR_ROW) {
            for (usize i = 0; i < children.count; ++i) {
                LayoutNode *child = layout_node_by_index(children.offset + i);
                child->x = pos_x;

                match(*child) {
                    case(LAYOUT_NODE_TEXT) assert(false && "currently not supported");
                    case(LAYOUT_NODE_CONTAINER, child_container) {
                        child->y = align_cross(child_container.config.style.align_self, node->h, style.padding, child->h);
                    }
                }

                pos_x += child->w + style.spacing;
            }
            break;
        }

        case(DIR_COL) {
            for (usize i = 0; i < children.count; ++i) {
                LayoutNode *child = layout_node_by_index(children.offset + i);
                child->y = pos_y;

                match(*child) {
                    case(LAYOUT_NODE_TEXT) assert(false && "currently not supported");
                    case(LAYOUT_NODE_CONTAINER, child_container) {
                        child->x = align_cross(child_container.config.style.align_self, node->w, style.padding, child->w);
                    }
                }

                pos_y += child->h + style.spacing;
            }
            break;
        }
    }

    for (usize i = 0; i < children.count; ++i) {
        LayoutNodeID child_id = Layout.children_persistent.items[children.offset + i];
        layout_positions(child_id);
    }
}

i32 align_cross(Alignment align, i32 parent_size, i32 parent_padding, i32 child_size) {
    i32 parent_inner = parent_size - 2 * parent_padding;
    match (align) {
        case(ALIGN_START)   return parent_padding;
        case(ALIGN_CENTER)  return parent_padding + (parent_inner - child_size) / 2;
        case(ALIGN_END)     return parent_padding + parent_inner - child_size;
    }

    UNREACHABLE("It must always match against alignment");
}

void container_commands(LayoutNode *node) {
    unwrap_into(*node, LAYOUT_NODE_CONTAINER, container);

    list_append(&Layout.cmds, 
        tag(LayoutCommand, LAYOUT_CMD_CLIP_START, {
            .x = node->x, .y = node->y, .w = node->w, .h = node->h
    }));

    list_append(&Layout.cmds,
        tag(LayoutCommand, LAYOUT_CMD_RECT, {
            .x = node->x, .y = node->y, .w = node->w, .h = node->h,
            .color = container.config.style.color
    }));

    ChildIdxSlice children = node->children;
    for (usize i = 0; i < children.count; ++i) {
        LayoutNodeID child_id = Layout.children_persistent.items[children.offset + i];
        layout_commands(child_id);
    }

    list_append(&Layout.cmds, tag0(LayoutCommand, LAYOUT_CMD_CLIP_END));
}

// void text_intrinsic_width(LayoutNode *node) {
//     unwrap_into(*node, LAYOUT_NODE_TEXT, text_container);
//     i32 text_width = 0;

//     List(CodePoint) text = text_container.text;
//     for (usize i = 0; i < text.count; ++i)
//         text_width += text.items[i].display_width;

//     node->w = text_width;
// }

#define X(ns, fn)   void ns##_##fn(LayoutNode *node) { UNUSED(node); }
xmacro(text)
#undef X

#endif