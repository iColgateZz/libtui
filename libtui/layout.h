#ifndef LIBTUI_LAYOUT_INCLUDE
#define LIBTUI_LAYOUT_INCLUDE

#define PSH_CORE_NO_PREFIX
#include "psh_core/psh_core.h"

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
} Layout_SizeStyle;

#define FIXED(value)    tag(Layout_SizeStyle, SIZE_FIXED, {value})
#define FIT(min, max)   tag(Layout_SizeStyle, SIZE_FIT, {min, max})
#define FILL(min, max)  tag(Layout_SizeStyle, SIZE_FILL, {min, max})

typedef struct {
    Layout_SizeStyle w;
    Layout_SizeStyle h;
} Layout_Sizing;

typedef struct {
    u8 r, g, b;
} Layout_Color;

typedef struct {
    i32 x, y, w, h;
} Layout_Rect;

typedef struct {
    packed_enum {
        DIR_ROW,
        DIR_COL,
    } type;
} Layout_Direction;

typedef struct {
    packed_enum {
        ALIGN_START,
        ALIGN_CENTER,
        ALIGN_END,
    } type;
} Layout_Alignment;

typedef enum {
    SCROLL_NONE,
    SCROLL_Y,
} Layout_Scroll;

typedef struct {
    Layout_Sizing size;
    Layout_Color color;
    u8 padding;
    u8 spacing;
    // u8 margin;
    // BorderStyle border;
    Layout_Direction direction;
    Layout_Alignment align_children;
    Layout_Alignment align_self;
    Layout_Scroll scroll;
} Layout_ContainerStyle;

typedef struct {
    Layout_ContainerStyle style;
    // void *userdata;
} Layout_ContainerConfig;

typedef struct {
    Layout_Color color;
} Layout_TextStyle;

typedef enum {
    LAYOUT_EVENT_MOUSE_LEFT,
    LAYOUT_EVENT_MOUSE_RIGHT,
    LAYOUT_EVENT_MOUSE_MIDDLE,
    LAYOUT_EVENT_SCROLL_UP,
    LAYOUT_EVENT_SCROLL_DOWN,
    LAYOUT_EVENT_MOUSE_DRAG,
    LAYOUT_EVENT_KEY,
    LAYOUT_EVENT_TEXT,
} Layout_EventType;

typedef struct {
    Layout_EventType type;
    union {
        struct {
            i32 x, y;
            b32 pressed;
        } mouse;
        i32 key;
        CodePoint text;
    };
} Layout_Event;

slice_def(CodePoint);

typedef struct {
    Slice(CodePoint) text;
    Layout_TextStyle style;
} Layout_TextConfig;

typedef struct {
    i32 offset;
    i32 count;
} Layout__ChildrenIndices;

typedef i32 Layout__TempID;
typedef i32 Layout_PersistentID;

typedef struct {
    //TODO: using i16 is probably more than enough
    Layout__TempID parent;
    Layout_PersistentID id;
    i32 x, y; // resolved coords
    i32 w, h; // resolved w, h
    i32 min_w, min_h;
    Layout__ChildrenIndices children;
    
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
            Layout_ContainerConfig config;
        } _LAYOUT_NODE_CONTAINER;
        struct {
            Layout_TextConfig config;
        } _LAYOUT_NODE_TEXT;
    };
} Layout__Node;

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
            Layout_Color color;
        } _LAYOUT_CMD_RECT;
        struct {
            i32 x, y;
            Slice(CodePoint) text;
            Layout_TextStyle style;
        } _LAYOUT_CMD_TEXT;
        struct {
            i32 x, y, w, h;
        } _LAYOUT_CMD_CLIP_START;
    };
} Layout_Command;

list_def(Layout_Command);
slice_def(Layout_Command);
list_def(Layout_Event);

void layout_begin(i32 w, i32 h);
Slice(Layout_Command) layout_end();
void layout_text_open(Layout_PersistentID id, Layout_TextConfig conf);
void layout_container_open(Layout_PersistentID id, Layout_ContainerConfig conf);
void layout_close();
void layout_event_push(Layout_Event event);

static u8 _latch = 0;

#define Container(id, ...)                              \
    for (_latch = (layout_container_open(               \
            id,                                         \
            (Layout_ContainerConfig) {                  \
                .style.size.w = FIT(0, INT32_MAX),      \
                .style.size.h = FIT(0, INT32_MAX),      \
                __VA_ARGS__                             \
        }), 0); _latch < 1; _latch = 1, layout_close())

#define Text(id, ...)              \
    for (_latch = (layout_text_open(id, (Layout_TextConfig) {__VA_ARGS__}), 0); _latch < 1; _latch = 1, layout_close())

#endif

#ifdef LIBTUI_LAYOUT_IMPL

list_def(Layout__Node);
list_def(Layout__TempID);
typedef Layout__Node* Layout__NodePtr;
list_def(Layout__NodePtr);

#define LAYOUT_TEMP_ID_NONE ((Layout__TempID)-1)
#define LAYOUT_PERSISTENT_ID_NONE 0

typedef struct {
    Layout_PersistentID id;
    i32 y;
    i32 max_y;
} Layout__ScrollState;

list_def(Layout__ScrollState);

typedef struct {
    List(Layout__Node) nodes;
    List(Layout__TempID) open_node_stack;
    List(Layout__TempID) temporary_child_stack;
    List(Layout__TempID) frame_children;
    List(Layout_Command) commands;
    List(Layout_Event) events;
    List(Layout__ScrollState) scroll_states;
    Layout__TempID hit_id_temp;
    Layout_PersistentID hit_id;
    Layout_PersistentID focused_id;
    Arena tmp;
} Layout__State;

static Layout__State layout__state = {0};

static inline Layout__Node *layout__node_from_temp_id(Layout__TempID id);
static inline Layout__Node *layout__node_from_index(i32 index);
static inline Layout__TempID layout__node_push(Layout__Node node);

void layout_event_push(Layout_Event event) { list_append(&layout__state.events, event); }

void layout_begin(i32 w, i32 h) {
    // reset state
    layout__state.nodes.count = 0;
    layout__state.open_node_stack.count = 0;
    layout__state.temporary_child_stack.count = 0;
    layout__state.frame_children.count = 0;
    layout__state.commands.count = 0;
    layout__state.hit_id_temp = LAYOUT_TEMP_ID_NONE;
    layout__state.hit_id = LAYOUT_PERSISTENT_ID_NONE;

    // open implicit root element
    layout_container_open(LAYOUT_PERSISTENT_ID_NONE, (Layout_ContainerConfig) {
        .style = {
            .size = {.w = FIXED(w), .h = FIXED(h)},
            .direction = {DIR_COL},
            .scroll = SCROLL_Y,
        }
    });
}

static inline void layout__node_layout(Layout__Node *node);

Slice(Layout_Command) layout_end() {
    // close implicit root element
    layout_close();

    #define ROOT_ID    0
    Layout__Node *root = layout__node_from_temp_id(ROOT_ID);
    layout__node_layout(root);

    layout__state.events.count = 0;

    return (Slice(Layout_Command)) {
        .items = layout__state.commands.items,
        .count = layout__state.commands.count,
    };
}

void layout_text_open(Layout_PersistentID id, Layout_TextConfig conf) {
    Layout__Node node = tag(Layout__Node, LAYOUT_NODE_TEXT, {.config = conf});
    node.id = id;
    Layout__TempID temp_id = layout__node_push(node);
    list_append(&layout__state.open_node_stack, temp_id);
}

void layout_container_open(Layout_PersistentID id, Layout_ContainerConfig conf) {
    Layout__Node node = tag(Layout__Node, LAYOUT_NODE_CONTAINER, {.config = conf});
    node.id = id;
    Layout__TempID temp_id = layout__node_push(node);
    list_append(&layout__state.open_node_stack, temp_id);
}

void layout_close() {
    Layout__TempID closed_id = list_pop(&layout__state.open_node_stack);
    Layout__Node *closed = layout__node_from_temp_id(closed_id);
    isize start = layout__state.temporary_child_stack.count - closed->children.count;
    isize end = layout__state.temporary_child_stack.count;
    closed->children.offset = layout__state.frame_children.count;
    // Take last IDs from the temp child stack
    // and attach them to contiguous child list
    layout__state.temporary_child_stack.count = start;
    for (isize i = start; i < end; ++i) {
        Layout__TempID child_id = layout__state.temporary_child_stack.items[i];
        list_append(&layout__state.frame_children, child_id);
        // Attach parent to child
        Layout__Node *child = layout__node_from_temp_id(child_id);
        child->parent = closed_id;
    }

    list_append(&layout__state.temporary_child_stack, closed_id);
    if (closed_id > ROOT_ID) {
        Layout__TempID parent_id = list_last(&layout__state.open_node_stack);
        Layout__Node *parent = layout__node_from_temp_id(parent_id);
        parent->children.count++;
    }
}

static inline void layout__node_intrinsic_width(Layout__Node *node);
static inline void layout__node_fill_width(Layout__Node *node);
static inline void layout__node_wrap_text(Layout__Node *node);
static inline void layout__node_intrinsic_height(Layout__Node *node);
static inline void layout__node_fill_height(Layout__Node *node);
static inline void layout__node_positions(Layout__Node *node);
static inline void layout__node_commands(Layout__Node *node);
static inline void layout__node_handle_events(Layout__Node *root);

static inline void layout__node_layout(Layout__Node *node) {
    layout__node_intrinsic_width(node);
    layout__node_fill_width(node);
    layout__node_wrap_text(node);
    layout__node_intrinsic_height(node);
    layout__node_fill_height(node);
    layout__node_positions(node);
    layout__node_commands(node);
    layout__node_handle_events(node);
}

/*
    fn                  | container | *generic  | text
    ---------------------------------------------------
    intrinsic_width     | yes       | yes       | yes
    fill_width          | yes       | yes       | no
    wrap_text           | yes       | no        | yes
    intrinsic_height    | yes       | yes       | no
    fill_height         | yes       | yes       | no
    positions           | yes       | no        | yes
    commands            | yes       | no        | yes
    handle_events       | no        | no        | no
*/

static inline void layout__container_intrinsic_width(Layout__Node *node);
static inline void layout__container_fill_width(Layout__Node *node);
static inline void layout__container_wrap_text(Layout__Node *node);
static inline void layout__container_intrinsic_height(Layout__Node *node);
static inline void layout__container_fill_height(Layout__Node *node);
static inline void layout__container_positions(Layout__Node *node);
static inline void layout__container_commands(Layout__Node *node);

static inline void layout__text_intrinsic_width(Layout__Node *node);
static inline void layout__text_wrap_text(Layout__Node *node);
static inline void layout__text_commands(Layout__Node *node);
static inline void layout__text_fill_width(Layout__Node *node) { UNUSED(node); }
static inline void layout__text_intrinsic_height(Layout__Node *node) { UNUSED(node); }
static inline void layout__text_fill_height(Layout__Node *node) { UNUSED(node); }
static inline void layout__text_positions(Layout__Node *node) { UNUSED(node); }

#define LAYOUT__NODE_DISPATCH(operation)                                      \
    static inline void layout__node_##operation(Layout__Node *node) {         \
        match(*node) {                                                        \
            case(LAYOUT_NODE_TEXT)                                            \
                layout__text_##operation(node);                               \
                break;                                                        \
                                                                              \
            case(LAYOUT_NODE_CONTAINER)                                       \
                layout__container_##operation(node);                          \
                break;                                                        \
                                                                              \
            otherwise UNREACHABLE("Unknown type");                            \
        }                                                                     \
    }

LAYOUT__NODE_DISPATCH(intrinsic_width)
LAYOUT__NODE_DISPATCH(fill_width)
LAYOUT__NODE_DISPATCH(wrap_text)
LAYOUT__NODE_DISPATCH(intrinsic_height)
LAYOUT__NODE_DISPATCH(fill_height)
LAYOUT__NODE_DISPATCH(positions)
LAYOUT__NODE_DISPATCH(commands)

#undef LAYOUT__NODE_DISPATCH

typedef enum {
    DIM_X,
    DIM_Y,
} Layout__Dimension;

typedef struct {
    i32 min;
    i32 max;
} Layout__SizeRange;

static inline b32 layout__event_is_mouse(Layout_Event event);
static inline b32 layout__rect_contains_point(i32 x, i32 y, Layout_Rect r);
static inline Layout_Rect layout__rect_intersect(Layout_Rect a, Layout_Rect b);
static inline Layout_Rect layout__rect_from_node(Layout__Node *node);
static inline Layout__Dimension layout__dimension_get_other(Layout__Dimension dim);
static inline Layout__Dimension layout__direction_get_main_dimension(Layout_Direction direction);
static inline i32 *layout__node_get_pos(Layout__Node *node, Layout__Dimension dim);
static inline i32 *layout__node_get_size(Layout__Node *node, Layout__Dimension dim);
static inline i32 *layout__node_get_min_size(Layout__Node *node, Layout__Dimension dim);
static inline Layout_SizeStyle layout__get_size_style(Layout_ContainerStyle style, Layout__Dimension dim);
static inline Layout__SizeRange layout__get_size_range(Layout_SizeStyle size);
static inline i32 layout__get_children_spacing(Layout__ChildrenIndices children, i32 spacing);
static inline b32 layout__node_is_fill(Layout__Node *node, Layout__Dimension dim);
static inline i32 layout__node_get_fill_max(Layout__Node *node, Layout__Dimension dim);
static inline i32 layout__node_get_fill_min(Layout__Node *node, Layout__Dimension dim);
static inline void layout__space_distribute(i32 space, List(Layout__NodePtr) nodes, Layout__Dimension dim);
static inline i32 layout__align_cross(Layout_Alignment align, i32 parent_size, i32 parent_padding, i32 child_size);
static inline i32 layout__align_along(Layout_Alignment align, i32 parent_size, i32 parent_padding, i32 children_size);
static inline Layout_Alignment layout__node_get_align_self(Layout__Node *node);
static inline b32 layout__node_scrolls_y(Layout__Node *node);
static inline Layout__ScrollState *layout__scroll_state_from_id(Layout_PersistentID id);

static inline void layout__node_intrinsic_size(Layout__Node *node, Layout__Dimension dim) {
    dim == DIM_X ? layout__node_intrinsic_width(node) : layout__node_intrinsic_height(node);
}

static inline void layout__container_intrinsic_size(Layout__Node *node, Layout__Dimension dim);
static inline void layout__container_intrinsic_width(Layout__Node *node) { layout__container_intrinsic_size(node, DIM_X); }
static inline void layout__container_intrinsic_height(Layout__Node *node) { layout__container_intrinsic_size(node, DIM_Y); }

static inline void layout__container_intrinsic_size(Layout__Node *node, Layout__Dimension dim) {
    unwrap_into(*node, LAYOUT_NODE_CONTAINER, container);

    Layout__ChildrenIndices children = node->children;
    for (isize i = 0; i < children.count; ++i) {
        layout__node_intrinsic_size(layout__node_from_index(children.offset + i), dim);
    }

    Layout_ContainerStyle style = container.config.style;
    Layout_SizeStyle size_style = layout__get_size_style(style, dim);
    if (matches(size_style, SIZE_FIXED)) {
        unwrap_into(size_style, SIZE_FIXED, fixed);
        *layout__node_get_size(node, dim) = *layout__node_get_min_size(node, dim) = fixed.value;
        return;
    }

    i32 resolved_size =     0;
    i32 resolved_min_size = 0;

    if (dim == layout__direction_get_main_dimension(style.direction)) {
        resolved_size =     2 * style.padding + layout__get_children_spacing(children, style.spacing);
        resolved_min_size = 2 * style.padding + layout__get_children_spacing(children, style.spacing);
        for (isize i = 0; i < children.count; ++i) {
            Layout__Node *child = layout__node_from_index(children.offset + i);
            resolved_size += *layout__node_get_size(child, dim);
            resolved_min_size += *layout__node_get_min_size(child, dim);
        }
    } else {
        for (isize i = 0; i < children.count; ++i) {
            Layout__Node *child = layout__node_from_index(children.offset + i);
            resolved_size = MAX(resolved_size, *layout__node_get_size(child, dim));
            resolved_min_size = MAX(resolved_min_size, *layout__node_get_min_size(child, dim));
        }
        resolved_size +=     2 * style.padding;
        resolved_min_size += 2 * style.padding;
    }

    Layout__SizeRange range = layout__get_size_range(size_style);
    *layout__node_get_size(node, dim) = CLAMP(resolved_size, range.min, range.max);
    *layout__node_get_min_size(node, dim) = CLAMP(resolved_min_size, range.min, range.max);
}

static inline void layout__text_intrinsic_width(Layout__Node *node) {
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

static inline void layout__node_fill_size(Layout__Node *node, Layout__Dimension dim) {
    dim == DIM_X ? layout__node_fill_width(node) : layout__node_fill_height(node);
}

static inline void layout__container_fill_size(Layout__Node *node, Layout__Dimension dim);
static inline void layout__container_fill_width(Layout__Node *node) { layout__container_fill_size(node, DIM_X); }
static inline void layout__container_fill_height(Layout__Node *node) { layout__container_fill_size(node, DIM_Y); }

static inline void layout__container_fill_size(Layout__Node *node, Layout__Dimension dim) {
    unwrap_into(*node, LAYOUT_NODE_CONTAINER, container);

    Layout_ContainerStyle style = container.config.style;
    Layout__ChildrenIndices children = node->children;

    if (dim == layout__direction_get_main_dimension(style.direction)) {
        i32 children_size = 0;
        for (isize i = 0; i < children.count; ++i) {
            children_size += *layout__node_get_size(layout__node_from_index(children.offset + i), dim);
        }

        i32 remaining_size = *layout__node_get_size(node, dim) - children_size - 2 * style.padding
                             - layout__get_children_spacing(children, style.spacing);

        i32 fill_count = 0;
        for (isize i = 0; i < children.count; ++i) {
            fill_count += layout__node_is_fill(layout__node_from_index(children.offset + i), dim);
        }

        if (fill_count > 0) {
            Scratch scratch = scratch_begin(&layout__state.tmp);
            List(Layout__NodePtr) fill_list = {
                .items = arena_push(scratch.arena, Layout__Node *, fill_count),
                .capacity = fill_count,
            };

            for (isize i = 0; i < children.count; ++i) {
                Layout__Node *child = layout__node_from_index(children.offset + i);
                if (layout__node_is_fill(child, dim)) list_append(&fill_list, child);
            }

            layout__space_distribute(remaining_size, fill_list, dim);
            scratch_end(scratch);
        }
    } else {
        i32 content_size = *layout__node_get_size(node, dim) - 2 * style.padding;
        for (isize i = 0; i < children.count; ++i) {
            Layout__Node *child = layout__node_from_index(children.offset + i);
            match(*child) {
                case(LAYOUT_NODE_TEXT) {
                    if (dim == DIM_X) {
                        *layout__node_get_size(child, dim) = MAX(content_size, *layout__node_get_min_size(child, dim));
                    }
                    break;
                }
                case(LAYOUT_NODE_CONTAINER, container) {
                    Layout_SizeStyle size_style = layout__get_size_style(container.config.style, dim);
                    if (matches(size_style, SIZE_FILL)) {
                        Layout__SizeRange range = layout__get_size_range(size_style);
                        *layout__node_get_size(child, dim) = CLAMP(content_size, MAX(range.min, *layout__node_get_min_size(child, dim)), range.max);
                    }
                    break;
                }
            }
        }
    }

    for (isize i = 0; i < children.count; ++i) {
        layout__node_fill_size(layout__node_from_index(children.offset + i), dim);
    }
}

static inline void layout__container_wrap_text(Layout__Node *node) {
    Layout__ChildrenIndices children = node->children;
    for (isize i = 0; i < children.count; ++i) {
        layout__node_wrap_text(layout__node_from_index(children.offset + i));
    }
}

static inline void layout__text_wrap_text(Layout__Node *node) {
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

static inline void layout__container_positions(Layout__Node *node) {
    unwrap_into(*node, LAYOUT_NODE_CONTAINER, container);

    Layout_ContainerStyle style = container.config.style;
    Layout__ChildrenIndices children = node->children;
    Layout__Dimension main_dim = layout__direction_get_main_dimension(style.direction);
    Layout__Dimension cross_dim = layout__dimension_get_other(main_dim);
    i32 children_main_size = layout__get_children_spacing(children, style.spacing);
    i32 content_bottom = node->y + style.padding;

    for (isize i = 0; i < children.count; ++i) {
        children_main_size += *layout__node_get_size(layout__node_from_index(children.offset + i), main_dim);
    }

    i32 cursor = *layout__node_get_pos(node, main_dim) + style.padding
                 + layout__align_along(style.align_children, *layout__node_get_size(node, main_dim), style.padding, children_main_size);

    for (isize i = 0; i < children.count; ++i) {
        Layout__Node *child = layout__node_from_index(children.offset + i);
        *layout__node_get_pos(child, main_dim) = cursor;
        *layout__node_get_pos(child, cross_dim) = *layout__node_get_pos(node, cross_dim) + layout__align_cross(
            layout__node_get_align_self(child),
            *layout__node_get_size(node, cross_dim),
            style.padding,
            *layout__node_get_size(child, cross_dim)
        );

        content_bottom = MAX(content_bottom, child->y + child->h);
        cursor += *layout__node_get_size(child, main_dim) + style.spacing;
    }

    if (layout__node_scrolls_y(node)) {
        Layout__ScrollState *scroll = layout__scroll_state_from_id(node->id);
        i32 content_h = MAX(content_bottom + style.padding - node->y, 0);
        scroll->max_y = MAX(content_h - node->h, 0);
        scroll->y = CLAMP(scroll->y, 0, scroll->max_y);

        for (isize i = 0; i < children.count; ++i) {
            Layout__Node *child = layout__node_from_index(children.offset + i);
            child->y -= scroll->y;
        }
    }

    for (isize i = 0; i < children.count; ++i) {
        layout__node_positions(layout__node_from_index(children.offset + i));
    }
}

static void layout__node_handle_event(Layout__Node *root, Layout_Event event);
static Layout__TempID layout__node_hit_test(Layout__Node *node, Layout_Rect parent_clip, i32 x, i32 y);
static void layout__scroll_update_from_event(Layout_Event event);

static inline void layout__node_handle_events(Layout__Node *root) {
    for (isize i = 0; i < layout__state.events.count; ++i) {
        layout__node_handle_event(root, layout__state.events.items[i]);
    }
}

static void layout__node_handle_event(Layout__Node *root, Layout_Event event) {
    //TODO: handle more events
    if (!layout__event_is_mouse(event)) return;

    Layout_Rect root_clip = layout__rect_from_node(root);
    layout__state.hit_id_temp = layout__node_hit_test(root, root_clip, event.mouse.x, event.mouse.y);
    layout__state.hit_id = layout__state.hit_id_temp == LAYOUT_TEMP_ID_NONE
                  ? LAYOUT_PERSISTENT_ID_NONE
                  : layout__node_from_temp_id(layout__state.hit_id_temp)->id;

    if (event.type == LAYOUT_EVENT_MOUSE_LEFT && event.mouse.pressed) {
        layout__state.focused_id = layout__state.hit_id;
    }

    layout__scroll_update_from_event(event);
}

static Layout__TempID layout__node_hit_test(Layout__Node *node, Layout_Rect parent_clip, i32 x, i32 y) {
    Layout_Rect node_rect = layout__rect_from_node(node);
    Layout_Rect clip = layout__rect_intersect(parent_clip, node_rect);
    if (!layout__rect_contains_point(x, y, clip)) return LAYOUT_TEMP_ID_NONE;

    match(*node) {
        case(LAYOUT_NODE_CONTAINER) {
            Layout__ChildrenIndices children = node->children;
            for (isize i = children.count - 1; i >= 0; --i) {
                Layout__Node *child = layout__node_from_index(children.offset + i);
                Layout__TempID hit = layout__node_hit_test(child, clip, x, y);
                if (hit != LAYOUT_TEMP_ID_NONE) return hit;
            }
            break;
        }
        case(LAYOUT_NODE_TEXT) break;
    }

    return node->id != LAYOUT_PERSISTENT_ID_NONE ? (Layout__TempID)(node - layout__state.nodes.items) : LAYOUT_TEMP_ID_NONE;
}

static void layout__scroll_update_from_event(Layout_Event event) {
    if (event.type != LAYOUT_EVENT_SCROLL_UP &&
        event.type != LAYOUT_EVENT_SCROLL_DOWN) return;
    if (layout__state.hit_id_temp == LAYOUT_TEMP_ID_NONE) return;

    i32 delta = event.type == LAYOUT_EVENT_SCROLL_DOWN ? 1 : -1;
    Layout__TempID current_id = layout__state.hit_id_temp;

    for (;;) {
        Layout__Node *current = layout__node_from_temp_id(current_id);
        if (layout__node_scrolls_y(current)) {
            Layout__ScrollState *scroll = layout__scroll_state_from_id(current->id);
            scroll->y = CLAMP(scroll->y + delta, 0, scroll->max_y);
            return;
        }

        if (current_id == ROOT_ID) break;
        current_id = current->parent;
    }
}

static inline void layout__container_commands(Layout__Node *node) {
    unwrap_into(*node, LAYOUT_NODE_CONTAINER, container);

    list_append(&layout__state.commands, 
        tag(Layout_Command, LAYOUT_CMD_CLIP_START, {
            .x = node->x, .y = node->y, .w = node->w, .h = node->h
    }));

    list_append(&layout__state.commands,
        tag(Layout_Command, LAYOUT_CMD_RECT, {
            .x = node->x, .y = node->y, .w = node->w, .h = node->h,
            .color = container.config.style.color
    }));

    Layout__ChildrenIndices children = node->children;
    for (isize i = 0; i < children.count; ++i) {
        Layout__Node *child = layout__node_from_index(children.offset + i);
        layout__node_commands(child);
    }

    list_append(&layout__state.commands, tag0(Layout_Command, LAYOUT_CMD_CLIP_END));
}

static inline void layout__text_commands(Layout__Node *node) {
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
            list_append(&layout__state.commands, tag(Layout_Command, LAYOUT_CMD_TEXT, {
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

    list_append(&layout__state.commands, tag(Layout_Command, LAYOUT_CMD_TEXT, {
        .x = node->x, .y = y,
        .text = {
            .items = text.items + line_start,
            .count = line_count,
        },
        .style = text_node.config.style,
    }));
}

// Helper functions

static inline Layout__Node *layout__node_from_temp_id(Layout__TempID id) {
    assert(0 <= id && id < layout__state.nodes.count);
    return &layout__state.nodes.items[id];
}

static inline Layout__TempID layout__temp_id_from_child_index(i32 index) {
    assert(0 <= index && index < layout__state.frame_children.count);
    return layout__state.frame_children.items[index];
}

static inline Layout__Node *layout__node_from_index(i32 index) {
    return layout__node_from_temp_id(layout__temp_id_from_child_index(index));
}

static inline Layout__TempID layout__node_push(Layout__Node node) {
    Layout__TempID id = layout__state.nodes.count;
    list_append(&layout__state.nodes, node);
    return id;
}

static inline b32 layout__event_is_mouse(Layout_Event event) {
    return event.type == LAYOUT_EVENT_MOUSE_LEFT ||
           event.type == LAYOUT_EVENT_MOUSE_RIGHT ||
           event.type == LAYOUT_EVENT_MOUSE_MIDDLE ||
           event.type == LAYOUT_EVENT_SCROLL_UP ||
           event.type == LAYOUT_EVENT_SCROLL_DOWN ||
           event.type == LAYOUT_EVENT_MOUSE_DRAG;
}

static inline b32 layout__rect_contains_point(i32 x, i32 y, Layout_Rect r) {
    return r.x <= x && x < r.x + r.w
        && r.y <= y && y < r.y + r.h;
}

static inline Layout_Rect layout__rect_intersect(Layout_Rect a, Layout_Rect b) {
    i32 x1 = MAX(a.x, b.x);
    i32 y1 = MAX(a.y, b.y);
    i32 x2 = MIN(a.x + a.w, b.x + b.w);
    i32 y2 = MIN(a.y + a.h, b.y + b.h);

    if (x2 <= x1 || y2 <= y1) {
        return (Layout_Rect) {0, 0, 0, 0};
    }

    return (Layout_Rect) {x1, y1, x2 - x1, y2 - y1};
}

static inline Layout_Rect layout__rect_from_node(Layout__Node *node) {
    return (Layout_Rect) {.x = node->x, .y = node->y, .w = node->w, .h = node->h};
}

static inline Layout_Alignment layout__node_get_align_self(Layout__Node *node) {
    match(*node) {
        case(LAYOUT_NODE_CONTAINER, container) return container.config.style.align_self;
        case(LAYOUT_NODE_TEXT) return (Layout_Alignment) {ALIGN_START};
    }
    UNREACHABLE("It must always match against alignment");
}

static inline i32 layout__align_cross(Layout_Alignment align, i32 parent_size, i32 parent_padding, i32 child_size) {
    i32 parent_inner = parent_size - 2 * parent_padding;
    match (align) {
        case(ALIGN_START)   return parent_padding;
        case(ALIGN_CENTER)  return parent_padding + (parent_inner - child_size) / 2;
        case(ALIGN_END)     return parent_padding + parent_inner - child_size;
    }

    UNREACHABLE("It must always match against alignment");
}

static inline i32 layout__align_along(Layout_Alignment align, i32 parent_size, i32 parent_padding, i32 children_size) {
    i32 parent_inner = parent_size - 2 * parent_padding;
    i32 remaining = MAX(parent_inner - children_size, 0);
    match (align) {
        case(ALIGN_START)   return 0;
        case(ALIGN_CENTER)  return remaining / 2;
        case(ALIGN_END)     return remaining;
    }

    UNREACHABLE("It must always match against alignment");
}

static inline Layout__Dimension layout__dimension_get_other(Layout__Dimension dim) {
    return dim == DIM_X ? DIM_Y : DIM_X;
}

static inline Layout__Dimension layout__direction_get_main_dimension(Layout_Direction direction) {
    switch (direction.type) {
        case DIR_ROW: return DIM_X;
        case DIR_COL: return DIM_Y;
    }
    UNREACHABLE("Unknown direction");
}

static inline i32 *layout__node_get_pos(Layout__Node *node, Layout__Dimension dim) {
    return dim == DIM_X ? &node->x : &node->y;
}

static inline i32 *layout__node_get_size(Layout__Node *node, Layout__Dimension dim) {
    return dim == DIM_X ? &node->w : &node->h;
}

static inline i32 *layout__node_get_min_size(Layout__Node *node, Layout__Dimension dim) {
    return dim == DIM_X ? &node->min_w : &node->min_h;
}

static inline Layout_SizeStyle layout__get_size_style(Layout_ContainerStyle style, Layout__Dimension dim) {
    return dim == DIM_X ? style.size.w : style.size.h;
}

static inline Layout__SizeRange layout__get_size_range(Layout_SizeStyle size) {
    switch (size.type) {
        case SIZE_FIXED: return (Layout__SizeRange) {size._SIZE_FIXED.value, size._SIZE_FIXED.value};
        case SIZE_FIT:   return (Layout__SizeRange) {size._SIZE_FIT.min, size._SIZE_FIT.max};
        case SIZE_FILL:  return (Layout__SizeRange) {size._SIZE_FILL.min, size._SIZE_FILL.max};
    }
    UNREACHABLE("Unknown size type");
}

static inline i32 layout__get_children_spacing(Layout__ChildrenIndices children, i32 spacing) {
    return MAX(children.count - 1, 0) * spacing;
}

static inline b32 layout__node_is_fill(Layout__Node *node, Layout__Dimension dim) {
    match(*node) {
        case(LAYOUT_NODE_TEXT) return dim == DIM_X;
        case(LAYOUT_NODE_CONTAINER, container) return matches(layout__get_size_style(container.config.style, dim), SIZE_FILL);
    }
    return false;
}

static inline i32 layout__node_get_fill_max(Layout__Node *node, Layout__Dimension dim) {
    match(*node) {
        case(LAYOUT_NODE_TEXT) return *layout__node_get_size(node, dim);
        case(LAYOUT_NODE_CONTAINER, container) {
            Layout_SizeStyle size = layout__get_size_style(container.config.style, dim);
            assert(matches(size, SIZE_FILL) && "function is only for fill size");
            return layout__get_size_range(size).max;
        }
    }
    return false;
}

static inline i32 layout__node_get_fill_min(Layout__Node *node, Layout__Dimension dim) {
    match(*node) {
        case(LAYOUT_NODE_TEXT) return *layout__node_get_min_size(node, dim);
        case(LAYOUT_NODE_CONTAINER, container) {
            Layout_SizeStyle size = layout__get_size_style(container.config.style, dim);
            assert(matches(size, SIZE_FILL) && "function is only for fill size");
            return MAX(layout__get_size_range(size).min, *layout__node_get_min_size(node, dim));
        }
    }
    return false;
}

static inline void layout__space_distribute(i32 space, List(Layout__NodePtr) nodes, Layout__Dimension dim) {
    while (space > 0) {
        i32 smallest = INT32_MAX;
        i32 next_smallest = INT32_MAX;
        i32 smallest_count = 0;

        for (isize i = 0; i < nodes.count; ++i) {
            Layout__Node *current = nodes.items[i];
            i32 current_size = *layout__node_get_size(current, dim);

            if (current_size >= layout__node_get_fill_max(current, dim)) continue;

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
            Layout__Node *current = nodes.items[i];
            i32 current_size = *layout__node_get_size(current, dim);
            if (current_size != smallest || current_size >= layout__node_get_fill_max(current, dim)) continue;
            target_growth = MIN(target_growth, layout__node_get_fill_max(current, dim) - current_size);
        }

        if (target_growth == 0) target_growth = 1;

        i64 total_growth = (i64)target_growth * (i64)smallest_count;
        i32 space_for_distribution = (i32)MIN((i64)space, total_growth);
        i32 each = space_for_distribution / smallest_count;
        i32 extra = space_for_distribution % smallest_count;

        for (isize i = 0; i < nodes.count; ++i) {
            Layout__Node *current = nodes.items[i];
            i32 *current_size = layout__node_get_size(current, dim);
            if (*current_size != smallest || *current_size >= layout__node_get_fill_max(current, dim)) continue;

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
            Layout__Node *current = nodes.items[i];
            i32 current_size = *layout__node_get_size(current, dim);

            if (current_size <= layout__node_get_fill_min(current, dim)) continue;

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
            Layout__Node *current = nodes.items[i];
            i32 current_size = *layout__node_get_size(current, dim);
            if (current_size != largest || current_size <= layout__node_get_fill_min(current, dim)) continue;
            target_shrink = MIN(target_shrink, current_size - layout__node_get_fill_min(current, dim));
        }

        if (target_shrink == 0) target_shrink = 1;

        i64 total_shrink = (i64)target_shrink * (i64)largest_count;
        i32 space_for_distribution = (i32)MIN((i64)-space, total_shrink);
        i32 each = space_for_distribution / largest_count;
        i32 extra = space_for_distribution % largest_count;

        for (isize i = 0; i < nodes.count; ++i) {
            Layout__Node *current = nodes.items[i];
            i32 *current_size = layout__node_get_size(current, dim);
            if (*current_size != largest || *current_size <= layout__node_get_fill_min(current, dim)) continue;

            i32 sub = each + (extra > 0);
            extra -= extra > 0;

            *current_size -= sub;
            space += sub;
        }
    }
}

static inline Layout__ScrollState *layout__scroll_state_from_id(Layout_PersistentID id) {
    for (isize i = 0; i < layout__state.scroll_states.count; ++i) {
        Layout__ScrollState *state = &layout__state.scroll_states.items[i];
        if (state->id == id) return state;
    }

    list_append(&layout__state.scroll_states, ((Layout__ScrollState) {.id = id}));
    return &list_last(&layout__state.scroll_states);
}

static inline b32 layout__node_scrolls_y(Layout__Node *node) {
    if (node->id == LAYOUT_PERSISTENT_ID_NONE) return false;

    match(*node) {
        case(LAYOUT_NODE_CONTAINER, container) {
            return container.config.style.scroll == SCROLL_Y;
        }
        case(LAYOUT_NODE_TEXT) return false;
    }
    return false;
}

#endif
