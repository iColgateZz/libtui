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

#define tag(T, tag, ...) ((T) {.type = tag, ._##tag = __VA_ARGS__})
#define tag0(T, tag) ((T) {.type = tag})

typedef enum {
    SIZE_FIT,
    SIZE_FILL,
    SIZE_FIXED,
} SizeMode;

typedef struct {
    SizeMode type;
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
    enum {
        DIR_ROW,
        DIR_COL,
    } type;
} Direction;

typedef struct {
    enum {
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

} TextStyle;

typedef i32 LayoutNodeID;
list_def(LayoutNodeID);

typedef enum {
    LAYOUT_NODE_CONTAINER,
    LAYOUT_NODE_TEXT,
} LayoutNodeType;

typedef struct {
    LayoutNodeID parent;
    i32 x, y; // resolved coords
    i32 w, h; // resolved w, h
    
    LayoutNodeType type;
    union {
        struct {
            List(LayoutNodeID) children;
            LayoutNodeStyle style;
            // void *userdata;
        } _LAYOUT_NODE_CONTAINER;
        struct {
            List(CodePoint) text;
            TextStyle style;
        } _LAYOUT_NODE_TEXT;
    };
} LayoutNode;

list_def(LayoutNode);

typedef enum {
    LAYOUT_CMD_RECT,
    // LAYOUT_CMD_TEXT,
    LAYOUT_CMD_CLIP_START,
    LAYOUT_CMD_CLIP_END,
    // LAYOUT_CMD_BORDER, // pass concrete codepoints to draw
} LayoutCommandType;

typedef struct {
    LayoutCommandType type;
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

b32 layout_cmd_next(LayoutCommand *out);
void layout(LayoutNodeID root);
void layout_set_screen_w(u32 w);
void layout_set_screen_h(u32 h);
// hit testing, event handling, and state updates
// happen after layout phase

#endif

#ifdef LIBTUI_LAYOUT_IMPL

list_def(LayoutCommand);

static struct {
    LayoutNodeID root;
    List(LayoutNode) nodes;
    Arena tmp;
    List(LayoutCommand) cmds;
    usize cmd_idx;
    i32 screen_w, screen_h;
} Layout = {0};

LayoutNode *layout_node_get(LayoutNodeID id) {
    assert(0 <= id && (u32)id < Layout.nodes.count);
    return &Layout.nodes.items[id];
}

LayoutNodeID layout_node_push(LayoutNode node) {
    LayoutNodeID id = Layout.nodes.count;
    list_append(&Layout.nodes, node);
    return id;
}

void layout_set_screen_w(u32 w) { Layout.screen_w = w; }
void layout_set_screen_h(u32 h) { Layout.screen_h = h; }

b32 layout_cmd_next(LayoutCommand *out) {
    if (Layout.cmd_idx >= Layout.cmds.count) {
        Layout.cmd_idx = 0;
        Layout.cmds.count = 0;
        return false;
    }

    *out = Layout.cmds.items[Layout.cmd_idx++];
    return true;
}

void layout_intrinsic_width(LayoutNodeID id);
void layout_fill_width(LayoutNodeID id);
void layout_intrinsic_height(LayoutNodeID id);
void layout_fill_height(LayoutNodeID id);
void layout_positions(LayoutNodeID id);
void layout_commands(LayoutNodeID id);

i32 align_cross(Alignment align, i32 parent_size, i32 parent_padding, i32 child_size);

void layout(LayoutNodeID id) {
    layout_intrinsic_width(id);
    layout_fill_width(id);

    layout_intrinsic_height(id);
    layout_fill_height(id);

    layout_positions(id);

    layout_commands(id);
    // other nodes in the pipeline go here..
}

void layout_intrinsic_width(LayoutNodeID id) {
    LayoutNode *node = layout_node_get(id);
    match(*node) {
        case(LAYOUT_NODE_TEXT, text_container)
            i32 text_width = 0;

            List(CodePoint) text = text_container.text;
            for (usize i = 0; i < text.count; ++i)
                text_width += text.items[i].display_width;

            node->w = text_width;
            break;

        case(LAYOUT_NODE_CONTAINER, container) {
            List(LayoutNodeID) children = container.children;
            for (usize i = 0; i < children.count; ++i)
                layout_intrinsic_width(children.items[i]);

            LayoutNodeStyle style = container.style;
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
                                LayoutNode *child = layout_node_get(children.items[i]);
                                children_width += child->w;
                            }
                            node->w = children_width + 2 * style.padding + 
                                    MAX(children.count - 1, 0) * style.spacing;
                            break;
                        }

                        case(DIR_COL) {
                            i32 max_child_w = 0;
                            for (usize i = 0; i < children.count; ++i) {
                                LayoutNode *child = layout_node_get(children.items[i]);
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
    }
}

void layout_fill_width(LayoutNodeID id) { UNUSED(id); }

void layout_intrinsic_height(LayoutNodeID id) {
    LayoutNode *node = layout_node_get(id);
    match(*node) {
        case(LAYOUT_NODE_TEXT, text) {
            UNUSED(text);
            break;
        }

        case(LAYOUT_NODE_CONTAINER, container) {
            List(LayoutNodeID) children = container.children;
            for (usize i = 0; i < children.count; ++i)
                layout_intrinsic_height(children.items[i]);

            LayoutNodeStyle style = container.style;
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
                                LayoutNode *child = layout_node_get(children.items[i]);
                                max_child_h = MAX(child->h, max_child_h);
                            }
                            
                            node->h = max_child_h + 2 * style.padding;
                            break;
                        }
                        case(DIR_COL) {
                            i32 children_height = 0;
                            for (usize i = 0; i < children.count; ++i) {
                                LayoutNode *child = layout_node_get(children.items[i]);
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
    }
}

void layout_fill_height(LayoutNodeID id) { UNUSED(id); }

void layout_positions(LayoutNodeID id) {
    LayoutNode *node = layout_node_get(id);
    match(*node) {
        case(LAYOUT_NODE_TEXT, text) {
            UNUSED(text);
            break;
        }

        case(LAYOUT_NODE_CONTAINER, container) {
            if (node->parent < 0)
                node->x = node->y = 0;

            LayoutNodeStyle style = container.style;
            i32 pos_x = node->x + style.padding;
            i32 pos_y = node->y + style.padding;

            List(LayoutNodeID) children = container.children;
            match(style.direction) {
                case(DIR_ROW) {
                    for (usize i = 0; i < children.count; ++i) {
                        LayoutNode *child = layout_node_get(children.items[i]);
                        child->x = pos_x;
                        match(*child) {
                            case(LAYOUT_NODE_TEXT) assert(false && "currently not supported");
                            case(LAYOUT_NODE_CONTAINER, child_container) {
                                child->y = align_cross(child_container.style.align_self, node->h, style.padding, child->h);
                            }
                        }

                        pos_x += child->w + style.spacing;
                    }
                    break;
                }
                case(DIR_COL) {
                    for (usize i = 0; i < children.count; ++i) {
                        LayoutNode *child = layout_node_get(children.items[i]);
                        match(*child) {
                            case(LAYOUT_NODE_TEXT) assert(false && "currently not supported");
                            case(LAYOUT_NODE_CONTAINER, child_container) {
                                child->x = align_cross(child_container.style.align_self, node->w, style.padding, child->w);
                            }
                        }
                        child->y = pos_y;

                        pos_y += child->h + style.spacing;
                    }
                    break;
                }
            }

            for (usize i = 0; i < children.count; ++i)
                layout_positions(children.items[i]);
        }
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

void layout_commands(LayoutNodeID id) {
    LayoutNode *node = layout_node_get(id);
    match(*node) {
        case(LAYOUT_NODE_TEXT, text) UNUSED(text); break;

        case(LAYOUT_NODE_CONTAINER, container) {
            list_append(&Layout.cmds, 
                tag(LayoutCommand, LAYOUT_CMD_CLIP_START, {
                    .x = node->x, .y = node->y, .w = node->w, .h = node->h
            }));

            list_append(&Layout.cmds,
                tag(LayoutCommand, LAYOUT_CMD_RECT, {
                    .x = node->x, .y = node->y, .w = node->w, .h = node->h,
                    .color = container.style.color
            }));

            List(LayoutNodeID) children = container.children;
            for (usize i = 0; i < children.count; ++i)
                layout_commands(children.items[i]);

            list_append(&Layout.cmds, tag0(LayoutCommand, LAYOUT_CMD_CLIP_END));
            break;
        }
    }
}

#endif