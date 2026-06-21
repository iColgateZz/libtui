#ifndef LIBTUI_LAYOUT_INCLUDE
#define LIBTUI_LAYOUT_INCLUDE

#define PSH_CORE_NO_PREFIX
#include "psh_core/psh_core.h"
#include "unicode.h"

// For debug purposes only
#include "renderer.h"

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
} SizeStyle;

#define FIXED(value)    tag(SizeStyle, SIZE_FIXED, {value})
#define FIT(min, max)   tag(SizeStyle, SIZE_FIT, {min, max})
#define FILL(min, max)  tag(SizeStyle, SIZE_FILL, {min, max})

typedef struct {
    SizeStyle w;
    SizeStyle h;
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

typedef enum {
    SCROLL_NONE,
    SCROLL_Y,
} Scroll;

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
    Scroll scroll;
} LayoutNodeStyle;

typedef struct {
    LayoutNodeStyle style;
    // void *userdata;
} ContainerConfig;

typedef struct {
    Color color;
} TextStyle;

slice_def(CodePoint);

typedef struct {
    Slice(CodePoint) text;
    TextStyle style;
} TextConfig;

typedef struct {
    i32 offset;
    i32 count;
} ChildrenIndices;

typedef i32 LayoutTempID;
typedef i32 LayoutPersistentID;

typedef struct {
    //TODO: using i16 is probably more than enough
    LayoutTempID parent;
    LayoutPersistentID id;
    i32 x, y; // resolved coords
    i32 w, h; // resolved w, h
    i32 min_w, min_h;
    ChildrenIndices children;
    
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
            TextConfig config;
        } _LAYOUT_NODE_TEXT;
    };
} LayoutNode;

typedef struct {
    packed_enum {
        LAYOUT_CMD_RECT,
        LAYOUT_CMD_TEXT,
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
            i32 x, y;
            Slice(CodePoint) text;
            TextStyle style;
        } _LAYOUT_CMD_TEXT;
        struct {
            i32 x, y, w, h;
        } _LAYOUT_CMD_CLIP_START;
    };
} LayoutCommand;

void debug_cmd(i32 x, i32 y, LayoutCommand cmd) {
    match(cmd) {
        case(LAYOUT_CMD_RECT, rect) {
            debug(x, y, "Rect {x: %d, y: %d, w: %d, h: %d, color: {...}}", rect.x, rect.y, rect.w, rect.h);
            break;
        }
        otherwise return;
    }
}

list_def(LayoutCommand);
slice_def(LayoutCommand);

void layout_begin(i32 w, i32 h);
Slice(LayoutCommand) layout_end();
void layout_open_text(LayoutPersistentID id, TextConfig conf);
void layout_open_container(LayoutPersistentID id, ContainerConfig conf);
void layout_close();

static u8 _latch = 0;

#define Container(id, ...)                              \
    for (_latch = (layout_open_container(               \
            id,                                         \
            (ContainerConfig) {                         \
                .style.size.w = FIT(0, INT32_MAX),      \
                .style.size.h = FIT(0, INT32_MAX),      \
                __VA_ARGS__                             \
        }), 0); _latch < 1; _latch = 1, layout_close())

#define Text(id, ...)              \
    for (_latch = (layout_open_text(id, (TextConfig) {__VA_ARGS__}), 0); _latch < 1; _latch = 1, layout_close())

#endif

#ifdef LIBTUI_LAYOUT_IMPL

list_def(LayoutNode);
list_def(LayoutTempID);
typedef LayoutNode* LayoutNodePtr;
list_def(LayoutNodePtr);

#define LAYOUT_TEMP_ID_NONE ((LayoutTempID)-1)
#define LAYOUT_PERSISTENT_ID_NONE 0

typedef struct {
    LayoutPersistentID id;
    i32 y;
    i32 max_y;
} ScrollState;

list_def(ScrollState);

static struct {
    List(LayoutNode) nodes;
    List(LayoutTempID) open_node_stack;
    List(LayoutTempID) temporary_child_stack;
    List(LayoutTempID) frame_children;
    List(LayoutCommand) commands;
    List(ScrollState) scroll_states;
    LayoutTempID hit_id_temp;
    LayoutPersistentID hit_id;
    LayoutPersistentID focused_id;
    Arena tmp;
} Layout = {0};

LayoutNode *node_by_id(LayoutTempID id) {
    assert(0 <= id && id < Layout.nodes.count);
    return &Layout.nodes.items[id];
}

LayoutTempID id_by_index(i32 index) {
    assert(0 <= index && index < Layout.frame_children.count);
    return Layout.frame_children.items[index];
}

LayoutNode *node_by_index(i32 index) {
    return node_by_id(id_by_index(index));
}

LayoutTempID node_push(LayoutNode node) {
    LayoutTempID id = Layout.nodes.count;
    list_append(&Layout.nodes, node);
    return id;
}

ScrollState *scroll_state_by_id(LayoutPersistentID id) {
    for (isize i = 0; i < Layout.scroll_states.count; ++i) {
        ScrollState *state = &Layout.scroll_states.items[i];
        if (state->id == id) return state;
    }

    list_append(&Layout.scroll_states, ((ScrollState) {.id = id}));
    return &list_last(&Layout.scroll_states);
}

static inline b32 layout_node_scrolls_y(LayoutNode *node) {
    if (node->id == LAYOUT_PERSISTENT_ID_NONE) return false;

    match(*node) {
        case(LAYOUT_NODE_CONTAINER, container) {
            return container.config.style.scroll == SCROLL_Y;
        }
        case(LAYOUT_NODE_TEXT) return false;
    }
    return false;
}


void layout_begin(i32 w, i32 h) {
    // reset state
    Layout.nodes.count = 0;
    Layout.open_node_stack.count = 0;
    Layout.temporary_child_stack.count = 0;
    Layout.frame_children.count = 0;
    Layout.commands.count = 0;
    Layout.hit_id_temp = LAYOUT_TEMP_ID_NONE;
    Layout.hit_id = LAYOUT_PERSISTENT_ID_NONE;

    // open implicit root element
    layout_open_container(LAYOUT_PERSISTENT_ID_NONE, (ContainerConfig) {
        .style = {
            .size = {.w = FIXED(w), .h = FIXED(h)},
            .direction = {DIR_COL},
        }
    });
}

void layout(LayoutNode *node);
static void layout_update_events(LayoutNode *root);

Slice(LayoutCommand) layout_end() {
    // close implicit root element
    layout_close();

    // layout all elements
    #define ROOT_ID    0
    LayoutNode *root = node_by_id(ROOT_ID);
    layout(root);
    layout_update_events(root);

    // emit commands for renderer
    return (Slice(LayoutCommand)) {
        .items = Layout.commands.items,
        .count = Layout.commands.count,
    };
}

void layout_open_text(LayoutPersistentID id, TextConfig conf) {
    LayoutNode node = tag(LayoutNode, LAYOUT_NODE_TEXT, {.config = conf});
    node.id = id;
    LayoutTempID temp_id = node_push(node);
    list_append(&Layout.open_node_stack, temp_id);
}

void layout_open_container(LayoutPersistentID id, ContainerConfig conf) {
    LayoutNode node = tag(LayoutNode, LAYOUT_NODE_CONTAINER, {.config = conf});
    node.id = id;
    LayoutTempID temp_id = node_push(node);
    list_append(&Layout.open_node_stack, temp_id);
}

void layout_close() {
    LayoutTempID closed_id = list_pop(&Layout.open_node_stack);
    LayoutNode *closed = node_by_id(closed_id);
    isize start = Layout.temporary_child_stack.count - closed->children.count;
    isize end = Layout.temporary_child_stack.count;
    closed->children.offset = Layout.frame_children.count;
    // Take last IDs from the temp child stack
    // and attach them to contiguous child list
    Layout.temporary_child_stack.count = start;
    for (isize i = start; i < end; ++i) {
        LayoutTempID child_id = Layout.temporary_child_stack.items[i];
        list_append(&Layout.frame_children, child_id);
        // Attach parent to child
        LayoutNode *child = node_by_id(child_id);
        child->parent = closed_id;
    }

    list_append(&Layout.temporary_child_stack, closed_id);
    if (closed_id > ROOT_ID) {
        LayoutTempID parent_id = list_last(&Layout.open_node_stack);
        LayoutNode *parent = node_by_id(parent_id);
        parent->children.count++;
    }
}

#define xmacro(namespace)               \
        X(namespace, intrinsic_width)   \
        X(namespace, fill_width)        \
        X(namespace, wrap_text)         \
        X(namespace, intrinsic_height)  \
        X(namespace, fill_height)       \
        X(namespace, positions)         \
        X(namespace, commands)

#define X(ns, fn)   static inline void ns##_##fn(LayoutNode *node);
xmacro(layout)
xmacro(container)
xmacro(text)
#undef X

void layout(LayoutNode *node) {
    #define X(ns, fn)   ns##_##fn(node);
    xmacro(layout)
    #undef X
}

#define X(ns, fn)                               \
static inline void ns##_##fn(LayoutNode *node) {       \
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

typedef enum {
    DIM_X,
    DIM_Y,
} Dimension;

typedef struct {
    i32 min;
    i32 max;
} SizeRange;

static inline Dimension dim_other(Dimension dim);
static inline Dimension direction_main_dim(Direction direction);
static inline i32 *pos(LayoutNode *node, Dimension dim);
static inline i32 *size(LayoutNode *node, Dimension dim);
static inline i32 *min_size(LayoutNode *node, Dimension dim);
static inline SizeStyle size_style(LayoutNodeStyle style, Dimension dim);
static inline SizeRange size_range(SizeStyle size);
static inline i32 child_spacing(ChildrenIndices children, i32 spacing);
static inline b32 is_fill(LayoutNode *node, Dimension dim);
static inline i32 fill_max(LayoutNode *node, Dimension dim);
static inline i32 fill_min(LayoutNode *node, Dimension dim);
static void distribute_space(i32 space, List(LayoutNodePtr) nodes, Dimension dim);
static inline i32 align_cross(Alignment align, i32 parent_size, i32 parent_padding, i32 childsize);
static inline i32 align_along(Alignment align, i32 parent_size, i32 parent_padding, i32 children_size);
static inline Alignment align_self(LayoutNode *node);

static inline void layout_intrinsic_size(LayoutNode *node, Dimension dim) {
    dim == DIM_X ? layout_intrinsic_width(node) : layout_intrinsic_height(node);
}

void container_intrinsic_size(LayoutNode *node, Dimension dim) {
    unwrap_into(*node, LAYOUT_NODE_CONTAINER, container);

    ChildrenIndices children = node->children;
    for (isize i = 0; i < children.count; ++i) {
        layout_intrinsic_size(node_by_index(children.offset + i), dim);
    }

    LayoutNodeStyle style = container.config.style;
    SizeStyle sstyle = size_style(style, dim);
    if (matches(sstyle, SIZE_FIXED)) {
        unwrap_into(sstyle, SIZE_FIXED, fixed);
        *size(node, dim) = *min_size(node, dim) = fixed.value;
        return;
    }

    i32 resolved_size =     0;
    i32 resolved_min_size = 0;

    if (dim == direction_main_dim(style.direction)) {
        resolved_size =     2 * style.padding + child_spacing(children, style.spacing);
        resolved_min_size = 2 * style.padding + child_spacing(children, style.spacing);
        for (isize i = 0; i < children.count; ++i) {
            LayoutNode *child = node_by_index(children.offset + i);
            resolved_size += *size(child, dim);
            resolved_min_size += *min_size(child, dim);
        }
    } else {
        for (isize i = 0; i < children.count; ++i) {
            LayoutNode *child = node_by_index(children.offset + i);
            resolved_size = MAX(resolved_size, *size(child, dim));
            resolved_min_size = MAX(resolved_min_size, *min_size(child, dim));
        }
        resolved_size +=     2 * style.padding;
        resolved_min_size += 2 * style.padding;
    }

    SizeRange range = size_range(sstyle);
    *size(node, dim) = CLAMP(resolved_size, range.min, range.max);
    *min_size(node, dim) = CLAMP(resolved_min_size, range.min, range.max);
}

static inline void container_intrinsic_width(LayoutNode *node) { container_intrinsic_size(node, DIM_X); }
static inline void container_intrinsic_height(LayoutNode *node) { container_intrinsic_size(node, DIM_Y); }

void text_intrinsic_width(LayoutNode *node) {
    unwrap_into(*node, LAYOUT_NODE_TEXT, text_node);
    i32 line_width = 0;
    i32 max_line_width = 0;
    i32 max_word_width = 0;
    i32 current_word_width = 0;

    Slice(CodePoint) text = text_node.config.text;
    for (isize i = 0; i < text.count; ++i) {
        CodePoint cp = text.items[i];
        if (cp_equal(cp, cp_from_byte('\n'))) {
            max_line_width = MAX(max_line_width, line_width);
            max_word_width = MAX(max_word_width, current_word_width);
            line_width = 0;
            current_word_width = 0;
            continue;
        }

        line_width += cp.display_width;

        if (cp_equal(cp, cp_from_byte(' '))) {
            max_word_width = MAX(max_word_width, current_word_width);
            current_word_width = 0;
        } else {
            current_word_width += cp.display_width;
        }
    }

    max_line_width = MAX(max_line_width, line_width);
    max_word_width = MAX(max_word_width, current_word_width);

    node->w = max_line_width;
    node->min_w = max_word_width;
}

static inline void text_intrinsic_height(LayoutNode *node) { UNUSED(node); }

static inline void layout_fill_size(LayoutNode *node, Dimension dim) {
    dim == DIM_X ? layout_fill_width(node) : layout_fill_height(node);
}

void container_fill_size(LayoutNode *node, Dimension dim) {
    unwrap_into(*node, LAYOUT_NODE_CONTAINER, container);

    LayoutNodeStyle style = container.config.style;
    ChildrenIndices children = node->children;

    if (dim == direction_main_dim(style.direction)) {
        i32 children_size = 0;
        for (isize i = 0; i < children.count; ++i) {
            children_size += *size(node_by_index(children.offset + i), dim);
        }

        i32 remaining_size = *size(node, dim) - children_size - 2 * style.padding
                             - child_spacing(children, style.spacing);

        i32 fill_count = 0;
        for (isize i = 0; i < children.count; ++i) {
            fill_count += is_fill(node_by_index(children.offset + i), dim);
        }

        if (fill_count > 0) {
            Scratch scratch = scratch_begin(&Layout.tmp);
            List(LayoutNodePtr) fill_list = {
                .items = arena_push(scratch.arena, LayoutNode *, fill_count),
                .capacity = fill_count,
            };

            for (isize i = 0; i < children.count; ++i) {
                LayoutNode *child = node_by_index(children.offset + i);
                if (is_fill(child, dim)) list_append(&fill_list, child);
            }

            distribute_space(remaining_size, fill_list, dim);
            scratch_end(scratch);
        }
    } else {
        i32 content_size = *size(node, dim) - 2 * style.padding;
        for (isize i = 0; i < children.count; ++i) {
            LayoutNode *child = node_by_index(children.offset + i);
            match(*child) {
                case(LAYOUT_NODE_TEXT) {
                    if (dim == DIM_X) {
                        *size(child, dim) = MAX(content_size, *min_size(child, dim));
                    }
                    break;
                }
                case(LAYOUT_NODE_CONTAINER, container) {
                    SizeStyle sstyle = size_style(container.config.style, dim);
                    if (matches(sstyle, SIZE_FILL)) {
                        SizeRange range = size_range(sstyle);
                        *size(child, dim) = CLAMP(content_size, MAX(range.min, *min_size(child, dim)), range.max);
                    }
                    break;
                }
            }
        }
    }

    for (isize i = 0; i < children.count; ++i) {
        layout_fill_size(node_by_index(children.offset + i), dim);
    }
}

static inline void container_fill_width(LayoutNode *node) { container_fill_size(node, DIM_X); }
static inline void container_fill_height(LayoutNode *node) { container_fill_size(node, DIM_Y); }

static inline void text_fill_width(LayoutNode *node) { UNUSED(node); }
static inline void text_fill_height(LayoutNode *node) { UNUSED(node); }

void container_wrap_text(LayoutNode *node) {
    ChildrenIndices children = node->children;
    for (isize i = 0; i < children.count; ++i) {
        layout_wrap_text(node_by_index(children.offset + i));
    }
}

void text_wrap_text(LayoutNode *node) {
    unwrap_into(*node, LAYOUT_NODE_TEXT, text_node);
    Slice(CodePoint) text = text_node.config.text;
    if (text.count == 0) {
        node->h = node->min_h = 0;
        return;
    }

    i32 max_width = MAX(node->w, 1);
    i32 lines = 1;
    i32 line_width = 0;

    for (isize i = 0; i < text.count; ++i) {
        CodePoint cp = text.items[i];
        if (cp_equal(cp, cp_from_byte('\n'))) {
            lines++;
            line_width = 0;
            continue;
        }

        if (line_width > 0 && line_width + cp.display_width > max_width) {
            lines++;
            line_width = 0;
        }

        line_width += cp.display_width;
    }

    node->h = node->min_h = lines;
}

void container_positions(LayoutNode *node) {
    unwrap_into(*node, LAYOUT_NODE_CONTAINER, container);

    LayoutNodeStyle style = container.config.style;
    ChildrenIndices children = node->children;
    Dimension main_dim = direction_main_dim(style.direction);
    Dimension cross_dim = dim_other(main_dim);
    i32 children_main_size = child_spacing(children, style.spacing);
    i32 content_bottom = node->y + style.padding;

    for (isize i = 0; i < children.count; ++i) {
        children_main_size += *size(node_by_index(children.offset + i), main_dim);
    }

    i32 cursor = *pos(node, main_dim) + style.padding
                 + align_along(style.align_children, *size(node, main_dim), style.padding, children_main_size);

    for (isize i = 0; i < children.count; ++i) {
        LayoutNode *child = node_by_index(children.offset + i);
        *pos(child, main_dim) = cursor;
        *pos(child, cross_dim) = *pos(node, cross_dim) + align_cross(
            align_self(child),
            *size(node, cross_dim),
            style.padding,
            *size(child, cross_dim)
        );

        content_bottom = MAX(content_bottom, child->y + child->h);
        cursor += *size(child, main_dim) + style.spacing;
    }

    if (layout_node_scrolls_y(node)) {
        ScrollState *scroll = scroll_state_by_id(node->id);
        i32 content_h = MAX(content_bottom + style.padding - node->y, 0);
        scroll->max_y = MAX(content_h - node->h, 0);
        scroll->y = CLAMP(scroll->y, 0, scroll->max_y);

        for (isize i = 0; i < children.count; ++i) {
            LayoutNode *child = node_by_index(children.offset + i);
            child->y -= scroll->y;
        }
    }

    for (isize i = 0; i < children.count; ++i) {
        layout_positions(node_by_index(children.offset + i));
    }
}

static inline void text_positions(LayoutNode *node) { UNUSED(node); }

static inline b32 layout_is_mouse_event(void) {
    return is_event(EScrollUp) ||
           is_event(EScrollDown) ||
           is_event(EMouseDrag) ||
           is_event(EMouseLeft) ||
           is_event(EMouseMiddle) ||
           is_event(EMouseRight);
}

static inline Rectangle layout_node_rect(LayoutNode *node) {
    return (Rectangle) {.x = node->x, .y = node->y, .w = node->w, .h = node->h};
}

static LayoutTempID layout_hit_test_node(LayoutNode *node, Rectangle parent_clip, i32 x, i32 y) {
    Rectangle node_rect = layout_node_rect(node);
    Rectangle clip = rect_intersect(parent_clip, node_rect);
    if (!point_in_rect(x, y, clip)) return LAYOUT_TEMP_ID_NONE;

    match(*node) {
        case(LAYOUT_NODE_CONTAINER) {
            ChildrenIndices children = node->children;
            for (isize i = children.count - 1; i >= 0; --i) {
                LayoutNode *child = node_by_index(children.offset + i);
                LayoutTempID hit = layout_hit_test_node(child, clip, x, y);
                if (hit != LAYOUT_TEMP_ID_NONE) return hit;
            }
            break;
        }
        case(LAYOUT_NODE_TEXT) break;
    }

    return node->id != LAYOUT_PERSISTENT_ID_NONE ? (LayoutTempID)(node - Layout.nodes.items) : LAYOUT_TEMP_ID_NONE;
}

static void layout_update_scroll_from_event(void) {
    if (!is_event(EScrollUp) && !is_event(EScrollDown)) return;
    if (Layout.hit_id_temp == LAYOUT_TEMP_ID_NONE) return;

    i32 delta = is_event(EScrollDown) ? 1 : -1;
    LayoutTempID current_id = Layout.hit_id_temp;

    while (current_id != LAYOUT_TEMP_ID_NONE) {
        LayoutNode *current = node_by_id(current_id);
        if (layout_node_scrolls_y(current)) {
            ScrollState *scroll = scroll_state_by_id(current->id);
            scroll->y = CLAMP(scroll->y + delta, 0, scroll->max_y);
            event_consume();
            return;
        }

        if (current_id == ROOT_ID) break;
        current_id = current->parent;
    }
}

static void layout_update_events(LayoutNode *root) {
    Layout.hit_id = LAYOUT_PERSISTENT_ID_NONE;
    Layout.hit_id_temp = LAYOUT_TEMP_ID_NONE;

    if (layout_is_mouse_event()) {
        Rectangle root_clip = layout_node_rect(root);
        Layout.hit_id_temp = layout_hit_test_node(root, root_clip, get_mouse_x(), get_mouse_y());
        if (Layout.hit_id_temp != LAYOUT_TEMP_ID_NONE) {
            Layout.hit_id = node_by_id(Layout.hit_id_temp)->id;
        }
    }

    if (is_event(EMouseLeft) && is_mouse_pressed()) {
        Layout.focused_id = Layout.hit_id;
    }

    layout_update_scroll_from_event();
}

void container_commands(LayoutNode *node) {
    unwrap_into(*node, LAYOUT_NODE_CONTAINER, container);

    list_append(&Layout.commands, 
        tag(LayoutCommand, LAYOUT_CMD_CLIP_START, {
            .x = node->x, .y = node->y, .w = node->w, .h = node->h
    }));

    list_append(&Layout.commands,
        tag(LayoutCommand, LAYOUT_CMD_RECT, {
            .x = node->x, .y = node->y, .w = node->w, .h = node->h,
            .color = container.config.style.color
    }));

    ChildrenIndices children = node->children;
    for (isize i = 0; i < children.count; ++i) {
        LayoutNode *child = node_by_index(children.offset + i);
        layout_commands(child);
    }

    list_append(&Layout.commands, tag0(LayoutCommand, LAYOUT_CMD_CLIP_END));
}

void text_commands(LayoutNode *node) {
    unwrap_into(*node, LAYOUT_NODE_TEXT, text_node);
    Slice(CodePoint) text = text_node.config.text;
    if (text.count == 0) return;

    i32 max_width = MAX(node->w, 1);
    i32 line_start = 0;
    i32 line_count = 0;
    i32 line_width = 0;
    i32 y = node->y;

    for (isize i = 0; i < text.count; ++i) {
        CodePoint cp = text.items[i];
        b32 force_break = cp_equal(cp, cp_from_byte('\n'));
        b32 wrap_break = line_width > 0 && line_width + cp.display_width > max_width;

        if (force_break || wrap_break) {
            list_append(&Layout.commands, tag(LayoutCommand, LAYOUT_CMD_TEXT, {
                .x = node->x, .y = y,
                .text = {
                    .items = text.items + line_start,
                    .count = line_count,
                },
                .style = text_node.config.style,
            }));

            y++;
            line_start = force_break ? i + 1 : i;
            line_count = 0;
            line_width = 0;
            if (force_break) continue;
        }

        line_count++;
        line_width += cp.display_width;
    }

    list_append(&Layout.commands, tag(LayoutCommand, LAYOUT_CMD_TEXT, {
        .x = node->x, .y = y,
        .text = {
            .items = text.items + line_start,
            .count = line_count,
        },
        .style = text_node.config.style,
    }));
}

static inline Alignment align_self(LayoutNode *node) {
    match(*node) {
        case(LAYOUT_NODE_CONTAINER, container) return container.config.style.align_self;
        case(LAYOUT_NODE_TEXT) return (Alignment) {ALIGN_START};
    }
    UNREACHABLE("It must always match against alignment");
}

static inline 
i32 align_cross(Alignment align, i32 parent_size, i32 parent_padding, i32 childsize) {
    i32 parent_inner = parent_size - 2 * parent_padding;
    match (align) {
        case(ALIGN_START)   return parent_padding;
        case(ALIGN_CENTER)  return parent_padding + (parent_inner - childsize) / 2;
        case(ALIGN_END)     return parent_padding + parent_inner - childsize;
    }

    UNREACHABLE("It must always match against alignment");
}

static inline 
i32 align_along(Alignment align, i32 parent_size, i32 parent_padding, i32 children_size) {
    i32 parent_inner = parent_size - 2 * parent_padding;
    i32 remaining = MAX(parent_inner - children_size, 0);
    match (align) {
        case(ALIGN_START)   return 0;
        case(ALIGN_CENTER)  return remaining / 2;
        case(ALIGN_END)     return remaining;
    }

    UNREACHABLE("It must always match against alignment");
}

static inline Dimension dim_other(Dimension dim) {
    return dim == DIM_X ? DIM_Y : DIM_X;
}

static inline Dimension direction_main_dim(Direction direction) {
    switch (direction.type) {
        case DIR_ROW: return DIM_X;
        case DIR_COL: return DIM_Y;
    }
    UNREACHABLE("Unknown direction");
}

static inline i32 *pos(LayoutNode *node, Dimension dim) {
    return dim == DIM_X ? &node->x : &node->y;
}

static inline i32 *size(LayoutNode *node, Dimension dim) {
    return dim == DIM_X ? &node->w : &node->h;
}

static inline i32 *min_size(LayoutNode *node, Dimension dim) {
    return dim == DIM_X ? &node->min_w : &node->min_h;
}

static inline SizeStyle size_style(LayoutNodeStyle style, Dimension dim) {
    return dim == DIM_X ? style.size.w : style.size.h;
}

static inline SizeRange size_range(SizeStyle size) {
    switch (size.type) {
        case SIZE_FIXED: return (SizeRange) {size._SIZE_FIXED.value, size._SIZE_FIXED.value};
        case SIZE_FIT:   return (SizeRange) {size._SIZE_FIT.min, size._SIZE_FIT.max};
        case SIZE_FILL:  return (SizeRange) {size._SIZE_FILL.min, size._SIZE_FILL.max};
    }
    UNREACHABLE("Unknown size type");
}

static inline i32 child_spacing(ChildrenIndices children, i32 spacing) {
    return MAX(children.count - 1, 0) * spacing;
}

static inline b32 is_fill(LayoutNode *node, Dimension dim) {
    match(*node) {
        case(LAYOUT_NODE_TEXT) return dim == DIM_X;
        case(LAYOUT_NODE_CONTAINER, container) return matches(size_style(container.config.style, dim), SIZE_FILL);
    }
    return false;
}

static inline i32 fill_max(LayoutNode *node, Dimension dim) {
    match(*node) {
        case(LAYOUT_NODE_TEXT) return *size(node, dim);
        case(LAYOUT_NODE_CONTAINER, container) {
            SizeStyle size = size_style(container.config.style, dim);
            assert(matches(size, SIZE_FILL) && "function is only for fill size");
            return size_range(size).max;
        }
    }
    return false;
}

static inline i32 fill_min(LayoutNode *node, Dimension dim) {
    match(*node) {
        case(LAYOUT_NODE_TEXT) return *min_size(node, dim);
        case(LAYOUT_NODE_CONTAINER, container) {
            SizeStyle size = size_style(container.config.style, dim);
            assert(matches(size, SIZE_FILL) && "function is only for fill size");
            return MAX(size_range(size).min, *min_size(node, dim));
        }
    }
    return false;
}

static void distribute_space(i32 space, List(LayoutNodePtr) nodes, Dimension dim) {
    while (space > 0) {
        i32 smallest = INT32_MAX;
        i32 next_smallest = INT32_MAX;
        i32 smallest_count = 0;

        for (isize i = 0; i < nodes.count; ++i) {
            LayoutNode *current = nodes.items[i];
            i32 current_size = *size(current, dim);

            if (current_size >= fill_max(current, dim)) continue;

            if (current_size < smallest) {
                smallest_count = 1;
                next_smallest = smallest;
                smallest = current_size;
            } else if (current_size == smallest) {
                smallest_count++;
            } else if (current_size < next_smallest) {
                next_smallest = current_size;
            }
        }

        if (smallest_count == 0) break;

        i32 target_growth = next_smallest - smallest;
        for (isize i = 0; i < nodes.count; ++i) {
            LayoutNode *current = nodes.items[i];
            i32 current_size = *size(current, dim);
            if (current_size != smallest || current_size >= fill_max(current, dim)) continue;
            target_growth = MIN(target_growth, fill_max(current, dim) - current_size);
        }

        if (target_growth == 0) target_growth = 1;

        i32 space_for_distribution = MIN(space, target_growth * smallest_count);
        i32 each = space_for_distribution / smallest_count;
        i32 extra = space_for_distribution % smallest_count;

        for (isize i = 0; i < nodes.count; ++i) {
            LayoutNode *current = nodes.items[i];
            i32 *current_size = size(current, dim);
            if (*current_size != smallest || *current_size >= fill_max(current, dim)) continue;

            i32 add = each + (extra > 0);
            extra -= extra > 0;

            *current_size += add;
            space -= add;
        }
    }

    while (space < 0) {
        i32 largest = 0;
        i32 next_largest = 0;
        i32 largest_count = 0;

        for (isize i = 0; i < nodes.count; ++i) {
            LayoutNode *current = nodes.items[i];
            i32 current_size = *size(current, dim);

            if (current_size <= fill_min(current, dim)) continue;

            if (current_size > largest) {
                largest_count = 1;
                next_largest = largest;
                largest = current_size;
            } else if (current_size == largest) {
                largest_count++;
            } else if (current_size > next_largest) {
                next_largest = current_size;
            }
        }

        if (largest_count == 0) break;

        i32 target_shrink = largest - next_largest;
        for (isize i = 0; i < nodes.count; ++i) {
            LayoutNode *current = nodes.items[i];
            i32 current_size = *size(current, dim);
            if (current_size != largest || current_size <= fill_min(current, dim)) continue;
            target_shrink = MIN(target_shrink, current_size - fill_min(current, dim));
        }

        if (target_shrink == 0) target_shrink = 1;

        i32 space_for_distribution = MIN(-space, target_shrink * largest_count);
        i32 each = space_for_distribution / largest_count;
        i32 extra = space_for_distribution % largest_count;

        for (isize i = 0; i < nodes.count; ++i) {
            LayoutNode *current = nodes.items[i];
            i32 *current_size = size(current, dim);
            if (*current_size != largest || *current_size <= fill_min(current, dim)) continue;

            i32 sub = each + (extra > 0);
            extra -= extra > 0;

            *current_size -= sub;
            space += sub;
        }
    }
}

#endif
