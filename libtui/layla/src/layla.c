#include "layla_internal.h"

static State state = {0};

void layla_screen_set_dimensions(i32 w, i32 h) {
    state.width = w;
    state.height = h;
}

void layla_cursor_set_position(i32 x, i32 y) {
    state.cursor_x = x;
    state.cursor_y = y;
}

Layla_PersistentID layla_cursor_get_hovered_id(void) {
    return state.hovered_persistent_id;
}

b32 layla_cursor_is_hovered(void) {
    if (state.open_node_stack.count == 0) return false;

    TempID current_id = list_last(&state.open_node_stack);
    Layla_PersistentID persistent_id = node_from_temp_id(current_id)->id;
    return persistent_id != LAYLA_PERSISTENT_ID_NONE &&
           persistent_id == state.hovered_persistent_id;
}

void layla_begin(void) {
    // reset state
    state.nodes.count = 0;
    state.open_node_stack.count = 0;
    state.temporary_child_stack.count = 0;
    state.frame_children.count = 0;
    state.commands.count = 0;
    state.hovered_temp_id = LAYLA_TEMP_ID_NONE;

    // open implicit root element
    layla_container_open(LAYLA_PERSISTENT_ID_NONE, (Layla_ContainerConfig) {
        .style = {
            .size = {
                .w = LAYLA_FIXED(state.width),
                .h = LAYLA_FIXED(state.height),
            },
            .direction = LAYLA_DIR_COL,
            .scroll = LAYLA_SCROLL_Y,
        }
    });
}

Layla_CommandSlice layla_end(void) {
    // close implicit root element
    layla_close();

    Node *root = node_from_temp_id(LAYLA_ROOT_ID);
    node_layout(root);
    hover_test();

    return (Layla_CommandSlice) {
        .items = state.commands.items,
        .count = state.commands.count,
    };
}

void layla_text_open(Layla_PersistentID id, Layla_TextConfig conf) {
    Node node = {
        .id = id,
        .type = LAYLA_NODE_TEXT,
        .as.text.config = conf,
    };
    TempID temp_id = node_push(node);
    list_append(&state.open_node_stack, temp_id);
}

void layla_container_open(Layla_PersistentID id, Layla_ContainerConfig conf) {
    Node node = {
        .id = id,
        .type = LAYLA_NODE_CONTAINER,
        .as.container.config = conf,
    };
    TempID temp_id = node_push(node);
    list_append(&state.open_node_stack, temp_id);
}

void layla_close(void) {
    TempID closed_id = list_pop(&state.open_node_stack);
    Node *closed = node_from_temp_id(closed_id);
    isize start = state.temporary_child_stack.count - closed->children.count;
    isize end = state.temporary_child_stack.count;
    closed->children.offset = state.frame_children.count;
    // Take last IDs from the temp child stack
    // and attach them to contiguous child list
    state.temporary_child_stack.count = start;
    for (isize i = start; i < end; ++i) {
        TempID child_id = state.temporary_child_stack.items[i];
        list_append(&state.frame_children, child_id);
        // Attach parent to child
        Node *child = node_from_temp_id(child_id);
        child->parent = closed_id;
    }

    list_append(&state.temporary_child_stack, closed_id);
    if (closed_id > LAYLA_ROOT_ID) {
        TempID parent_id = list_last(&state.open_node_stack);
        Node *parent = node_from_temp_id(parent_id);
        parent->children.count++;
    }
}

static inline void node_layout(Node *node) {
    container_intrinsic_width(node);
    container_fill_width(node);
    container_wrap_text(node);
    container_intrinsic_height(node);
    container_fill_height(node);
    container_positions(node);
    container_commands(node);
}

static inline void container_intrinsic_width(Node *node) {
    ChildrenIndices children = node->children;
    for (isize i = 0; i < children.count; ++i) {
        Node *child = node_from_index(children.offset + i);
        if (child->type == LAYLA_NODE_TEXT) {
            text_intrinsic_width(child);
        } else {
            container_intrinsic_width(child);
        }
    }

    container_intrinsic_size(node, DIM_X);
}

static inline void container_intrinsic_height(Node *node) {
    ChildrenIndices children = node->children;
    for (isize i = 0; i < children.count; ++i) {
        Node *child = node_from_index(children.offset + i);
        if (child->type == LAYLA_NODE_CONTAINER) container_intrinsic_height(child);
    }

    container_intrinsic_size(node, DIM_Y);
}

static inline void container_intrinsic_size(Node *node, Dimension dim) {
    ChildrenIndices children = node->children;
    Layla_ContainerStyle style = node->as.container.config.style;
    Layla_SizeStyle size_style = get_size_style(style, dim);
    if (size_style.type == LAYLA_SIZE_FIXED) {
        *node_get_size(node, dim) = *node_get_min_size(node, dim) = size_style.as.fixed.value;
        return;
    }

    i32 resolved_size =     0;
    i32 resolved_min_size = 0;

    if (dim == direction_get_main_dimension(style.direction)) {
        resolved_size =     2 * style.padding + get_children_spacing(children, style.spacing);
        resolved_min_size = 2 * style.padding + get_children_spacing(children, style.spacing);
        for (isize i = 0; i < children.count; ++i) {
            Node *child = node_from_index(children.offset + i);
            resolved_size += *node_get_size(child, dim);
            resolved_min_size += *node_get_min_size(child, dim);
        }
    } else {
        for (isize i = 0; i < children.count; ++i) {
            Node *child = node_from_index(children.offset + i);
            resolved_size = MAX(resolved_size, *node_get_size(child, dim));
            resolved_min_size = MAX(resolved_min_size, *node_get_min_size(child, dim));
        }
        resolved_size +=     2 * style.padding;
        resolved_min_size += 2 * style.padding;
    }

    SizeRange range = get_size_range(size_style);
    *node_get_size(node, dim) = CLAMP(resolved_size, range.min, range.max);
    *node_get_min_size(node, dim) = CLAMP(resolved_min_size, range.min, range.max);
}

static inline void text_intrinsic_width(Node *node) {
    TextMeasurement measurement = text_process(node, 0, false);
    node->w = measurement.natural_width;
    node->min_w = measurement.minimum_width;
}

static inline void container_fill_width(Node *node) {
    container_fill_size(node, DIM_X);

    ChildrenIndices children = node->children;
    for (isize i = 0; i < children.count; ++i) {
        Node *child = node_from_index(children.offset + i);
        if (child->type == LAYLA_NODE_CONTAINER) container_fill_width(child);
    }
}

static inline void container_fill_height(Node *node) {
    container_fill_size(node, DIM_Y);

    ChildrenIndices children = node->children;
    for (isize i = 0; i < children.count; ++i) {
        Node *child = node_from_index(children.offset + i);
        if (child->type == LAYLA_NODE_CONTAINER) container_fill_height(child);
    }
}

static inline void container_fill_size(Node *node, Dimension dim) {
    Layla_ContainerStyle style = node->as.container.config.style;
    ChildrenIndices children = node->children;

    if (dim == direction_get_main_dimension(style.direction)) {
        i32 children_size = 0;
        for (isize i = 0; i < children.count; ++i) {
            children_size += *node_get_size(node_from_index(children.offset + i), dim);
        }

        i32 remaining_size = *node_get_size(node, dim) - children_size - 2 * style.padding
                             - get_children_spacing(children, style.spacing);

        i32 fill_count = 0;
        for (isize i = 0; i < children.count; ++i) {
            fill_count += node_is_fill(node_from_index(children.offset + i), dim);
        }

        if (fill_count > 0) {
            Scratch scratch = scratch_begin(&state.tmp);
            List(NodePtr) fill_list = {
                .items = arena_push(scratch.arena, Node *, fill_count),
                .capacity = fill_count,
            };

            for (isize i = 0; i < children.count; ++i) {
                Node *child = node_from_index(children.offset + i);
                if (node_is_fill(child, dim)) list_append(&fill_list, child);
            }

            space_distribute(remaining_size, fill_list, dim);
            scratch_end(scratch);
        }
    } else {
        i32 content_size = *node_get_size(node, dim) - 2 * style.padding;
        for (isize i = 0; i < children.count; ++i) {
            Node *child = node_from_index(children.offset + i);
            switch (child->type) {
                case LAYLA_NODE_TEXT: {
                    if (dim == DIM_X) {
                        *node_get_size(child, dim) = MAX(content_size, *node_get_min_size(child, dim));
                    }
                    break;
                }
                case LAYLA_NODE_CONTAINER: {
                    Layla_SizeStyle size_style = get_size_style(child->as.container.config.style, dim);
                    if (size_style.type == LAYLA_SIZE_FILL) {
                        SizeRange range = get_size_range(size_style);
                        *node_get_size(child, dim) = CLAMP(content_size, MAX(range.min, *node_get_min_size(child, dim)), range.max);
                    }
                    break;
                }
            }
        }
    }
}

static inline void container_wrap_text(Node *node) {
    ChildrenIndices children = node->children;
    for (isize i = 0; i < children.count; ++i) {
        Node *child = node_from_index(children.offset + i);
        if (child->type == LAYLA_NODE_TEXT) {
            text_wrap_text(child);
        } else {
            container_wrap_text(child);
        }
    }
}

static inline void text_wrap_text(Node *node) {
    TextMeasurement measurement = text_process(node, MAX(node->w, 1), false);
    node->h = node->min_h = measurement.line_count;
}

static inline void container_positions(Node *node) {
    Layla_ContainerStyle style = node->as.container.config.style;
    ChildrenIndices children = node->children;
    Dimension main_dim = direction_get_main_dimension(style.direction);
    Dimension cross_dim = dimension_get_other(main_dim);
    i32 children_main_size = get_children_spacing(children, style.spacing);
    i32 content_bottom = node->y + style.padding;

    for (isize i = 0; i < children.count; ++i) {
        children_main_size += *node_get_size(node_from_index(children.offset + i), main_dim);
    }

    i32 cursor = *node_get_pos(node, main_dim) + style.padding
                 + align_along(style.align_children, *node_get_size(node, main_dim), style.padding, children_main_size);

    for (isize i = 0; i < children.count; ++i) {
        Node *child = node_from_index(children.offset + i);
        *node_get_pos(child, main_dim) = cursor;
        *node_get_pos(child, cross_dim) = *node_get_pos(node, cross_dim) + align_cross(
            node_get_align_self(child),
            *node_get_size(node, cross_dim),
            style.padding,
            *node_get_size(child, cross_dim)
        );

        content_bottom = MAX(content_bottom, child->y + child->h);
        cursor += *node_get_size(child, main_dim) + style.spacing;
    }

    if (node_is_scroll_y(node)) {
        ScrollState *scroll = scroll_state_from_id(node->id);
        i32 content_h = MAX(content_bottom + style.padding - node->y, 0);
        scroll->max_y = MAX(content_h - node->h, 0);
        scroll->y = CLAMP(scroll->y, 0, scroll->max_y);

        for (isize i = 0; i < children.count; ++i) {
            Node *child = node_from_index(children.offset + i);
            child->y -= scroll->y;
        }
    }

    for (isize i = 0; i < children.count; ++i) {
        Node *child = node_from_index(children.offset + i);
        if (child->type == LAYLA_NODE_CONTAINER) container_positions(child);
    }
}

static TempID node_hit_test(Node *node, Layla_Rectangle parent_clip, i32 x, i32 y) {
    Layla_Rectangle node_rect = rect_from_node(node);
    Layla_Rectangle clip = rect_intersect(parent_clip, node_rect);
    if (!rect_contains_point(x, y, clip)) return LAYLA_TEMP_ID_NONE;

    ChildrenIndices children = node->children;
    for (isize i = children.count - 1; i >= 0; --i) {
        Node *child = node_from_index(children.offset + i);
        TempID hit = node_hit_test(child, clip, x, y);
        if (hit != LAYLA_TEMP_ID_NONE) return hit;
    }

    return node->id != LAYLA_PERSISTENT_ID_NONE ? (TempID)(node - state.nodes.items) : LAYLA_TEMP_ID_NONE;
}

static inline void hover_test(void) {
    Node *root = node_from_temp_id(LAYLA_ROOT_ID);
    Layla_Rectangle root_clip = rect_from_node(root);
    state.hovered_temp_id = node_hit_test(
        root, root_clip,
        state.cursor_x, state.cursor_y
    );

    if (state.hovered_temp_id == LAYLA_TEMP_ID_NONE) return;

    state.hovered_persistent_id = node_from_temp_id(state.hovered_temp_id)->id;
}

void layla_scroll_update(i32 delta_y) {
    TempID current_id = state.hovered_temp_id;
    if (delta_y == 0 || current_id == LAYLA_TEMP_ID_NONE) return;

    for (;;) {
        Node *current = node_from_temp_id(current_id);
        if (node_is_scroll_y(current)) {
            ScrollState *scroll = scroll_state_from_id(current->id);
            scroll->y = CLAMP(scroll->y + delta_y, 0, scroll->max_y);
            return;
        }

        if (current_id == LAYLA_ROOT_ID) break;
        current_id = current->parent;
    }
}

static inline void container_commands(Node *node) {
    list_append(&state.commands,
        ((Layla_Command) {.type = LAYLA_CMD_CLIP_START, .as.clip_start = {
            .x = node->x, .y = node->y, .w = node->w, .h = node->h
        }})
    );

    list_append(&state.commands,
        ((Layla_Command) {.type = LAYLA_CMD_RECTANGLE, .as.rectangle = {
            .x = node->x, .y = node->y, .w = node->w, .h = node->h,
            .color = node->as.container.config.style.color
        }})
    );

    ChildrenIndices children = node->children;
    for (isize i = 0; i < children.count; ++i) {
        Node *child = node_from_index(children.offset + i);
        if (child->type == LAYLA_NODE_TEXT) {
            text_commands(child);
        } else {
            container_commands(child);
        }
    }

    list_append(&state.commands, ((Layla_Command) {
        .type = LAYLA_CMD_CLIP_END,
    }));
}

static inline void text_commands(Node *node) {
    text_process(node, MAX(node->w, 1), true);
}

static inline TextMeasurement text_process(Node *node, i32 wrap_width, b32 emit_commands) {
    Layla_TextConfig config = node->as.text.config;
    Layla_TextSlice source = config.text;
    TextMeasurement measurement = {0};
    if (source.count <= 0) return measurement;

    switch (config.wrap_policy) {
        case LAYLA_TEXT_WRAP_WORD: break;
        default: UNREACHABLE("Unknown text wrapping policy");
    }

    isize cursor_byte = 0;
    isize line_start_byte = 0;
    isize line_end_byte = 0;
    i32 line_width = 0;
    i32 unwrapped_line_width = 0;
    i32 pending_space_width = 0;
    b32 line_has_word = false;

    byte *source_end = source.items + source.count;
    while (cursor_byte < source.count) {
        if (source.items[cursor_byte] == '\n') {
            line_width += pending_space_width;
            pending_space_width = 0;
            line_end_byte = cursor_byte;
            measurement.natural_width = MAX(measurement.natural_width, unwrapped_line_width);

            if (emit_commands) {
                append_text_command(
                    node, config.style, source,
                    line_start_byte, line_end_byte,
                    line_width, node->y + measurement.line_count
                );
            }

            measurement.line_count++;
            cursor_byte++;
            line_start_byte = cursor_byte;
            line_end_byte = cursor_byte;
            line_width = 0;
            unwrapped_line_width = 0;
            line_has_word = false;
            continue;
        }

        if (source.items[cursor_byte] == ' ') {
            isize spaces_start_byte = cursor_byte;
            while (cursor_byte < source.count && source.items[cursor_byte] == ' ') cursor_byte++;

            i32 spaces_width = cursor_byte - spaces_start_byte;
            pending_space_width += spaces_width;
            unwrapped_line_width += spaces_width;
            continue;
        }

        isize word_start_byte = cursor_byte;
        i32 word_width = 0;
        while (cursor_byte < source.count &&
               source.items[cursor_byte] != ' ' &&
               source.items[cursor_byte] != '\n') {
            byte *decode_cursor = source.items + cursor_byte;
            CodePoint codepoint = utf8_next(&decode_cursor, source_end);
            cursor_byte = decode_cursor - source.items;
            word_width += codepoint.display_width;
        }

        unwrapped_line_width += word_width;
        measurement.minimum_width = MAX(measurement.minimum_width, word_width);

        i32 width_with_word = line_width + pending_space_width + word_width;
        b32 word_overflows_line = wrap_width > 0 && line_has_word && width_with_word > wrap_width;
        if (word_overflows_line) {
            if (emit_commands) {
                append_text_command(
                    node, config.style, source,
                    line_start_byte, line_end_byte,
                    line_width, node->y + measurement.line_count
                );
            }

            measurement.line_count++;
            line_start_byte = word_start_byte;
            line_width = word_width;
        } else {
            line_width = width_with_word;
        }

        line_end_byte = cursor_byte;
        pending_space_width = 0;
        line_has_word = true;
    }

    measurement.natural_width = MAX(measurement.natural_width, unwrapped_line_width);
    line_width += pending_space_width;
    line_end_byte = source.count;

    if (emit_commands) {
        append_text_command(
            node, config.style, source,
            line_start_byte, line_end_byte,
            line_width, node->y + measurement.line_count
        );
    }
    measurement.line_count++;

    return measurement;
}

static inline void append_text_command(
    Node *node,
    Layla_TextStyle style,
    Layla_TextSlice source,
    isize line_start_byte,
    isize line_end_byte,
    i32 line_width,
    i32 line_y
) {
    i32 remaining_width = MAX(node->w - line_width, 0);
    i32 line_x = node->x;
    switch (node->as.text.config.alignment) {
        case LAYLA_ALIGN_START: break;
        case LAYLA_ALIGN_CENTER: line_x += remaining_width / 2; break;
        case LAYLA_ALIGN_END: line_x += remaining_width; break;
        default: UNREACHABLE("Unknown text alignment");
    }

    list_append(&state.commands,
        ((Layla_Command) {.type = LAYLA_CMD_TEXT, .as.text = {
            .x = line_x,
            .y = line_y,
            .text_slice = {
                .items = source.items + line_start_byte,
                .count = line_end_byte - line_start_byte,
            },
            .style = style,
        }})
    );
}

// Helper functions

static inline Node *node_from_temp_id(TempID id) {
    assert(0 <= id && id < state.nodes.count);
    return &state.nodes.items[id];
}

static inline TempID temp_id_from_child_index(i32 index) {
    assert(0 <= index && index < state.frame_children.count);
    return state.frame_children.items[index];
}

static inline Node *node_from_index(i32 index) {
    return node_from_temp_id(temp_id_from_child_index(index));
}

static inline TempID node_push(Node node) {
    TempID id = state.nodes.count;
    list_append(&state.nodes, node);
    return id;
}

static inline b32 rect_contains_point(i32 x, i32 y, Layla_Rectangle r) {
    return r.x <= x && x < r.x + r.w
        && r.y <= y && y < r.y + r.h;
}

static inline Layla_Rectangle rect_intersect(Layla_Rectangle a, Layla_Rectangle b) {
    i32 x1 = MAX(a.x, b.x);
    i32 y1 = MAX(a.y, b.y);
    i32 x2 = MIN(a.x + a.w, b.x + b.w);
    i32 y2 = MIN(a.y + a.h, b.y + b.h);

    if (x2 <= x1 || y2 <= y1) {
        return (Layla_Rectangle) {0, 0, 0, 0};
    }

    return (Layla_Rectangle) {x1, y1, x2 - x1, y2 - y1};
}

static inline Layla_Rectangle rect_from_node(Node *node) {
    return (Layla_Rectangle) {.x = node->x, .y = node->y, .w = node->w, .h = node->h};
}

static inline Layla_Alignment node_get_align_self(Node *node) {
    switch (node->type) {
        case LAYLA_NODE_CONTAINER: return node->as.container.config.style.align_self;
        case LAYLA_NODE_TEXT: return LAYLA_ALIGN_START;
    }
    UNREACHABLE("It must always match against alignment");
}

static inline i32 align_cross(Layla_Alignment align, i32 parent_size, i32 parent_padding, i32 child_size) {
    i32 parent_inner = parent_size - 2 * parent_padding;
    switch (align) {
        case LAYLA_ALIGN_START:  return parent_padding;
        case LAYLA_ALIGN_CENTER: return parent_padding + (parent_inner - child_size) / 2;
        case LAYLA_ALIGN_END:    return parent_padding + parent_inner - child_size;
    }

    UNREACHABLE("It must always match against alignment");
}

static inline i32 align_along(Layla_Alignment align, i32 parent_size, i32 parent_padding, i32 children_size) {
    i32 parent_inner = parent_size - 2 * parent_padding;
    i32 remaining = MAX(parent_inner - children_size, 0);
    switch (align) {
        case LAYLA_ALIGN_START:  return 0;
        case LAYLA_ALIGN_CENTER: return remaining / 2;
        case LAYLA_ALIGN_END:    return remaining;
    }

    UNREACHABLE("It must always match against alignment");
}

static inline Dimension dimension_get_other(Dimension dim) {
    return dim == DIM_X ? DIM_Y : DIM_X;
}

static inline Dimension direction_get_main_dimension(Layla_Direction direction) {
    switch (direction) {
        case LAYLA_DIR_ROW: return DIM_X;
        case LAYLA_DIR_COL: return DIM_Y;
    }
    UNREACHABLE("Unknown direction");
}

static inline i32 *node_get_pos(Node *node, Dimension dim) {
    return dim == DIM_X ? &node->x : &node->y;
}

static inline i32 *node_get_size(Node *node, Dimension dim) {
    return dim == DIM_X ? &node->w : &node->h;
}

static inline i32 *node_get_min_size(Node *node, Dimension dim) {
    return dim == DIM_X ? &node->min_w : &node->min_h;
}

static inline Layla_SizeStyle get_size_style(Layla_ContainerStyle style, Dimension dim) {
    return dim == DIM_X ? style.size.w : style.size.h;
}

static inline SizeRange get_size_range(Layla_SizeStyle size) {
    switch (size.type) {
        case LAYLA_SIZE_FIXED: return (SizeRange) {size.as.fixed.value, size.as.fixed.value};
        case LAYLA_SIZE_FIT:   return (SizeRange) {size.as.fit.min, size.as.fit.max};
        case LAYLA_SIZE_FILL:  return (SizeRange) {size.as.fill.min, size.as.fill.max};
    }
    UNREACHABLE("Unknown size type");
}

static inline i32 get_children_spacing(ChildrenIndices children, i32 spacing) {
    return MAX(children.count - 1, 0) * spacing;
}

static inline b32 node_is_fill(Node *node, Dimension dim) {
    switch (node->type) {
        case LAYLA_NODE_TEXT: return dim == DIM_X;
        case LAYLA_NODE_CONTAINER:
            return get_size_style(node->as.container.config.style, dim).type == LAYLA_SIZE_FILL;
    }
    return false;
}

static inline i32 node_get_fill_max(Node *node, Dimension dim) {
    switch (node->type) {
        case LAYLA_NODE_TEXT: return *node_get_size(node, dim);
        case LAYLA_NODE_CONTAINER: {
            Layla_SizeStyle size = get_size_style(node->as.container.config.style, dim);
            assert(size.type == LAYLA_SIZE_FILL && "function is only for fill size");
            return get_size_range(size).max;
        }
    }
    return false;
}

static inline i32 node_get_fill_min(Node *node, Dimension dim) {
    switch (node->type) {
        case LAYLA_NODE_TEXT: return *node_get_min_size(node, dim);
        case LAYLA_NODE_CONTAINER: {
            Layla_SizeStyle size = get_size_style(node->as.container.config.style, dim);
            assert(size.type == LAYLA_SIZE_FILL && "function is only for fill size");
            return MAX(get_size_range(size).min, *node_get_min_size(node, dim));
        }
    }
    return false;
}

static inline void space_distribute(i32 space, List(NodePtr) nodes, Dimension dim) {
    while (space > 0) {
        i32 smallest = INT32_MAX;
        i32 next_smallest = INT32_MAX;
        i32 smallest_count = 0;

        for (isize i = 0; i < nodes.count; ++i) {
            Node *current = nodes.items[i];
            i32 current_size = *node_get_size(current, dim);

            if (current_size >= node_get_fill_max(current, dim)) continue;

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
            Node *current = nodes.items[i];
            i32 current_size = *node_get_size(current, dim);
            if (current_size != smallest || current_size >= node_get_fill_max(current, dim)) continue;
            target_growth = MIN(target_growth, node_get_fill_max(current, dim) - current_size);
        }

        if (target_growth == 0) target_growth = 1;

        i64 total_growth = (i64)target_growth * (i64)smallest_count;
        i32 space_for_distribution = (i32)MIN((i64)space, total_growth);
        i32 each = space_for_distribution / smallest_count;
        i32 extra = space_for_distribution % smallest_count;

        for (isize i = 0; i < nodes.count; ++i) {
            Node *current = nodes.items[i];
            i32 *current_size = node_get_size(current, dim);
            if (*current_size != smallest || *current_size >= node_get_fill_max(current, dim)) continue;

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
            Node *current = nodes.items[i];
            i32 current_size = *node_get_size(current, dim);

            if (current_size <= node_get_fill_min(current, dim)) continue;

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
            Node *current = nodes.items[i];
            i32 current_size = *node_get_size(current, dim);
            if (current_size != largest || current_size <= node_get_fill_min(current, dim)) continue;
            target_shrink = MIN(target_shrink, current_size - node_get_fill_min(current, dim));
        }

        if (target_shrink == 0) target_shrink = 1;

        i64 total_shrink = (i64)target_shrink * (i64)largest_count;
        i32 space_for_distribution = (i32)MIN((i64)-space, total_shrink);
        i32 each = space_for_distribution / largest_count;
        i32 extra = space_for_distribution % largest_count;

        for (isize i = 0; i < nodes.count; ++i) {
            Node *current = nodes.items[i];
            i32 *current_size = node_get_size(current, dim);
            if (*current_size != largest || *current_size <= node_get_fill_min(current, dim)) continue;

            i32 sub = each + (extra > 0);
            extra -= extra > 0;

            *current_size -= sub;
            space += sub;
        }
    }
}

static inline ScrollState *scroll_state_from_id(Layla_PersistentID id) {
    for (isize i = 0; i < state.scroll_states.count; ++i) {
        ScrollState *scroll_state = &state.scroll_states.items[i];
        if (scroll_state->id == id) return scroll_state;
    }

    list_append(&state.scroll_states, ((ScrollState) {.id = id}));
    return &list_last(&state.scroll_states);
}

static inline b32 node_is_scroll_y(Node *node) {
    if (node->id == LAYLA_PERSISTENT_ID_NONE) return false;

    switch (node->type) {
        case LAYLA_NODE_CONTAINER: {
            return node->as.container.config.style.scroll == LAYLA_SCROLL_Y;
        }
        case LAYLA_NODE_TEXT: return false;
    }
    return false;
}
