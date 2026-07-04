#include "layla_internal.h"

static Layout__State layout__state = {0};

void layout_screen_set_dimensions(i32 w, i32 h) {
    layout__state.width = w;
    layout__state.height = h;
}

void layout_cursor_set_position(i32 x, i32 y) {
    layout__state.cursor_x = x;
    layout__state.cursor_y = y;
}

Layout_PersistentID layout_cursor_get_hovered_id() {
    return layout__state.hovered_persistent_id;
}

b32 layout_cursor_is_hovered() {
    if (layout__state.open_node_stack.count == 0) return false;

    Layout__TempID current_id = list_last(&layout__state.open_node_stack);
    Layout_PersistentID persistent_id = layout__node_from_temp_id(current_id)->id;
    return persistent_id != LAYOUT_PERSISTENT_ID_NONE &&
           persistent_id == layout__state.hovered_persistent_id;
}

void layout_begin() {
    // reset state
    layout__state.nodes.count = 0;
    layout__state.open_node_stack.count = 0;
    layout__state.temporary_child_stack.count = 0;
    layout__state.frame_children.count = 0;
    layout__state.commands.count = 0;
    layout__state.hovered_temp_id = LAYOUT_TEMP_ID_NONE;

    // open implicit root element
    layout_container_open(LAYOUT_PERSISTENT_ID_NONE, (Layout_ContainerConfig) {
        .style = {
            .size = {
                .w = FIXED(layout__state.width),
                .h = FIXED(layout__state.height),
            },
            .direction = {DIR_COL},
            .scroll = SCROLL_Y,
        }
    });
}

Layout_CommandSlice layout_end() {
    // close implicit root element
    layout_close();

    Layout__Node *root = layout__node_from_temp_id(LAYOUT_ROOT_ID);
    layout__node_layout(root);
    layout__hover_test();

    return (Layout_CommandSlice) {
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
    if (closed_id > LAYOUT_ROOT_ID) {
        Layout__TempID parent_id = list_last(&layout__state.open_node_stack);
        Layout__Node *parent = layout__node_from_temp_id(parent_id);
        parent->children.count++;
    }
}

static inline void layout__node_layout(Layout__Node *node) {
    layout__node_intrinsic_width(node);
    layout__node_fill_width(node);
    layout__node_wrap_text(node);
    layout__node_intrinsic_height(node);
    layout__node_fill_height(node);
    layout__node_positions(node);
    layout__node_commands(node);
}

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

static inline void layout__node_intrinsic_size(Layout__Node *node, Layout__Dimension dim) {
    dim == DIM_X ? layout__node_intrinsic_width(node) : layout__node_intrinsic_height(node);
}

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

    Layout_TextSlice text = text_node.config.text;
    byte *p = text.items;
    byte *end = text.items + text.count;
    while (p < end) {
        CodePoint cp = utf8_next(&p, end);
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
    Layout_TextSlice text = text_node.config.text;
    if (text.count <= 0) {
        node->h = node->min_h = 0;
        return;
    }

    i32 max_width = MAX(node->w, 1);
    i32 lines = 1;
    i32 line_width = 0;

    byte *p = text.items;
    byte *end = text.items + text.count;
    while (p < end) {
        CodePoint cp = utf8_next(&p, end);
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

    if (layout__node_is_scroll_y(node)) {
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

static inline void layout__hover_test() {
    Layout__Node *root = layout__node_from_temp_id(LAYOUT_ROOT_ID);
    Layout_Rect root_clip = layout__rect_from_node(root);
    layout__state.hovered_temp_id = layout__node_hit_test(
        root, root_clip,
        layout__state.cursor_x, layout__state.cursor_y
    );

    if (layout__state.hovered_temp_id == LAYOUT_TEMP_ID_NONE) return;

    layout__state.hovered_persistent_id = layout__node_from_temp_id(layout__state.hovered_temp_id)->id;
}

void layout_scroll_update(i32 delta_y) {
    Layout__TempID current_id = layout__state.hovered_temp_id;
    if (delta_y == 0 || current_id == LAYOUT_TEMP_ID_NONE) return;

    for (;;) {
        Layout__Node *current = layout__node_from_temp_id(current_id);
        if (layout__node_is_scroll_y(current)) {
            Layout__ScrollState *scroll = layout__scroll_state_from_id(current->id);
            scroll->y = CLAMP(scroll->y + delta_y, 0, scroll->max_y);
            return;
        }

        if (current_id == LAYOUT_ROOT_ID) break;
        current_id = current->parent;
    }
}

static inline void layout__container_commands(Layout__Node *node) {
    unwrap_into(*node, LAYOUT_NODE_CONTAINER, container);

    list_append(&layout__state.commands, 
        ((Layout_Command) {.type = LAYOUT_CMD_CLIP_START, .as.clip_start = {
            .x = node->x, .y = node->y, .w = node->w, .h = node->h
        }})
    );

    list_append(&layout__state.commands,
        ((Layout_Command) {.type = LAYOUT_CMD_RECTANGLE, .as.rectangle = {
            .x = node->x, .y = node->y, .w = node->w, .h = node->h,
            .color = container.config.style.color
        }})
    );

    Layout__ChildrenIndices children = node->children;
    for (isize i = 0; i < children.count; ++i) {
        Layout__Node *child = layout__node_from_index(children.offset + i);
        layout__node_commands(child);
    }

    list_append(&layout__state.commands, tag0(Layout_Command, LAYOUT_CMD_CLIP_END));
}

static inline void layout__append_text_command(
    Layout__Node *node,
    Layout_TextStyle style,
    Layout_TextSlice source,
    isize line_start_byte,
    isize line_end_byte,
    i32 line_y
) {
    list_append(&layout__state.commands,
        ((Layout_Command) {.type = LAYOUT_CMD_TEXT, .as.text = {
            .x = node->x,
            .y = line_y,
            .text_slice = {
                .items = source.items + line_start_byte,
                .count = line_end_byte - line_start_byte,
            },
            .style = style,
        }})
    );
}

static inline void layout__text_commands(Layout__Node *node) {
    unwrap_into(*node, LAYOUT_NODE_TEXT, text_node);
    Layout_TextSlice source = text_node.config.text;
    if (source.count <= 0) return;

    i32 available_line_width = MAX(node->w, 1);
    isize line_start_byte = 0;
    i32 line_cell_width = 0;
    i32 line_y = node->y;

    isize next_byte = 0;
    byte *source_end = source.items + source.count;
    while (next_byte < source.count) {
        isize codepoint_start_byte = next_byte;
        byte *decode_cursor = source.items + next_byte;
        CodePoint codepoint = utf8_next(&decode_cursor, source_end);
        next_byte = decode_cursor - source.items;

        if (cp_equal(codepoint, cp_from_byte('\n'))) {
            layout__append_text_command(
                node, text_node.config.style, source,
                line_start_byte, codepoint_start_byte, line_y
            );

            line_start_byte = next_byte;
            line_cell_width = 0;
            line_y++;
            continue;
        }

        b32 codepoint_overflows_line =
            line_cell_width > 0 &&
            line_cell_width + codepoint.display_width > available_line_width;

        if (codepoint_overflows_line) {
            layout__append_text_command(
                node, text_node.config.style, source,
                line_start_byte, codepoint_start_byte, line_y
            );

            line_start_byte = codepoint_start_byte;
            line_cell_width = 0;
            line_y++;
        }

        line_cell_width += codepoint.display_width;
    }

    layout__append_text_command(
        node, text_node.config.style, source,
        line_start_byte, source.count, line_y
    );
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

static inline b32 layout__node_is_scroll_y(Layout__Node *node) {
    if (node->id == LAYOUT_PERSISTENT_ID_NONE) return false;

    match(*node) {
        case(LAYOUT_NODE_CONTAINER, container) {
            return container.config.style.scroll == SCROLL_Y;
        }
        case(LAYOUT_NODE_TEXT) return false;
    }
    return false;
}
