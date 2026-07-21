#include "layla_internal.h"

#ifdef LAYLA_STATIC_STORAGE
static Node NODES[LAYLA_MAX_NODES];
static TempID OPEN_NODE_STACK[LAYLA_MAX_NODES];
static TempID TEMP_CHILD_STACK[LAYLA_MAX_NODES];
static TempID FRAME_CHILDREN[LAYLA_MAX_NODES];
static TempID FLOATING_ROOTS[LAYLA_MAX_NODES];
static Layla_Command COMMANDS[LAYLA_MAX_COMMANDS];
static Layla_Error ERRORS[LAYLA_MAX_ERRORS];
static Layla_ElementID HOVERED_ELEMENT_IDS[LAYLA_MAX_NODES];
static ScrollState SCROLL_STATES[LAYLA_MAX_SCROLL_STATES];
static union { // try different alignments
    void *pointer;
    long double maximum_alignment;
    byte bytes[LAYLA_TEMP_STORAGE_SIZE];
} TMP_STORE;

static State state = {
    .nodes = { .items = NODES, .capacity = LAYLA_MAX_NODES },
    .open_node_stack = { .items = OPEN_NODE_STACK, .capacity = LAYLA_MAX_NODES },
    .temporary_child_stack = { .items = TEMP_CHILD_STACK, .capacity = LAYLA_MAX_NODES },
    .frame_children = { .items = FRAME_CHILDREN, .capacity = LAYLA_MAX_NODES },
    .floating_roots = { .items = FLOATING_ROOTS, .capacity = LAYLA_MAX_NODES },
    .commands = { .items = COMMANDS, .capacity = LAYLA_MAX_COMMANDS },
    .errors = { .items = ERRORS, .capacity = LAYLA_MAX_ERRORS },
    .hovered_element_ids = { .items = HOVERED_ELEMENT_IDS, .capacity = LAYLA_MAX_NODES },
    .scroll_states = { .items = SCROLL_STATES, .capacity = LAYLA_MAX_SCROLL_STATES },
    .cursor = {.x = -1, .y = -1},
    .tmp = {
        .base_ptr = TMP_STORE.bytes,
        .reserved_size = LAYLA_TEMP_STORAGE_SIZE,
        .committed_size = LAYLA_TEMP_STORAGE_SIZE,
        .page_size = LAYLA_TEMP_STORAGE_SIZE,
    },
};

#define layla_list_append(list, item) do {          \
        assert((list)->count < (list)->capacity);   \
        (list)->items[(list)->count++] = (item);    \
    } while (0)
#else //!LAYLA_STATIC_STORAGE
static State state = {.cursor = {.x = -1, .y = -1}};
#define layla_list_append list_append
#endif //LAYLA_STATIC_STORAGE

#define layla_list_pop  list_pop
#define layla_list_last list_last

static inline void error_emit(Layla_ErrorType type, Layla_ElementID id, byte const *message) {
    Layla_Error error = {
        .type = type,
        .id = id,
        .message = message,
    };

    layla_list_append(&state.errors, error);

    if (state.error_handler != NULL) {
        state.error_handler(error, state.error_handler_userdata);
    }
}

void layla_state_set_error_handler(Layla_ErrorHandler handler, void *userdata) {
    state.error_handler = handler;
    state.error_handler_userdata = userdata;
}

Layla_ErrorSlice layla_state_get_errors(void) {
    return (Layla_ErrorSlice) {
        .items = state.errors.items,
        .count = state.errors.count,
    };
}

void layla_state_set_text_measure_function(Layla_TextMeasureFunction function, void *userdata) {
    state.text_measure_function = function;
    state.text_measure_userdata = userdata;
}

void layla_state_set_screen_dimensions(i32 w, i32 h) {
    state.width = w;
    state.height = h;
}

void layla_state_set_cursor_state(i32 x, i32 y, b32 is_down) {
    state.cursor.x = x;
    state.cursor.y = y;

    if (is_down) {
        state.cursor.interaction_state =
            state.cursor.interaction_state == LAYLA_CURSOR_RELEASED ||
            state.cursor.interaction_state == LAYLA_CURSOR_RELEASED_THIS_FRAME
                ? LAYLA_CURSOR_PRESSED_THIS_FRAME
                : LAYLA_CURSOR_PRESSED;
    } else {
        state.cursor.interaction_state =
            state.cursor.interaction_state == LAYLA_CURSOR_PRESSED ||
            state.cursor.interaction_state == LAYLA_CURSOR_PRESSED_THIS_FRAME
                ? LAYLA_CURSOR_RELEASED_THIS_FRAME
                : LAYLA_CURSOR_RELEASED;
    }

    if (state.nodes.count == 0) {
        state.hovered_temp_id = LAYLA_TEMP_ID_NONE;
        state.hovered_element_ids.count = 0;
        return;
    }

    hover_test();
}

Layla_CursorState layla_state_get_cursor_state(void) {
    return state.cursor;
}

void layla_layout_begin(void) {
    // reset state
    state.nodes.count = 0;
    state.open_node_stack.count = 0;
    state.temporary_child_stack.count = 0;
    state.frame_children.count = 0;
    state.floating_roots.count = 0;
    state.commands.count = 0;
    state.hovered_temp_id = LAYLA_TEMP_ID_NONE;
    state.errors.count = 0;

    if (state.width <= 0 || state.height <= 0) {
        error_emit(
            LAYLA_ERROR_SCREEN_DIMENSIONS_NOT_SET,
            LAYLA_ELEMENT_ID_NONE,
            "Layla screen dimensions must be set to positive values before layout"
        );
    }

    // open implicit root element
    node_open((Node) {.type = LAYLA_NODE_CONTAINER});
    layla_container_element_configure((Layla_ContainerConfig) {
        .style = {
            .size = {
                .w = LAYLA_FIXED(state.width),
                .h = LAYLA_FIXED(state.height),
            },
            .direction = LAYLA_DIR_COL,
        }
    });
}

Layla_CommandSlice layla_layout_end(void) {
    // close implicit root element
    layla_element_close();

    Node *root = node_from_temp_id(LAYLA_ROOT_TEMP_ID);
    container_intrinsic_width(root);
    container_fill_width(root);
    container_wrap_text(root);
    container_intrinsic_height(root);
    container_fill_height(root);
    container_positions(root);
    container_commands(root);

    floating_roots_sort();
    for (isize i = 0; i < state.floating_roots.count; ++i)
        floating_layout(node_from_temp_id(state.floating_roots.items[i]));

    return (Layla_CommandSlice) {
        .items = state.commands.items,
        .count = state.commands.count,
    };
}

static inline void node_open(Node node) {
    if (state.open_node_stack.count > 0) {
        Node *parent = node_from_temp_id(layla_list_last(&state.open_node_stack));
        u32 offset = parent->next_child_id_offset++;
        if (node.id == LAYLA_ELEMENT_ID_NONE) {
            u32 hash = parent->id;
            hash += offset + 48;
            hash += hash << 10;
            hash ^= hash >> 6;
            hash += hash << 3;
            hash ^= hash >> 11;
            hash += hash << 15;
            node.id = hash + 1;
        }
    }

    TempID temp_id = node_push(node);
    layla_list_append(&state.open_node_stack, temp_id);
}

void layla_text_element_open(void) {
    assert(state.open_node_stack.count > 0);
    node_open((Node) {.type = LAYLA_NODE_TEXT});
}

void layla_text_element_open_with_id(Layla_ElementID id) {
    assert(state.open_node_stack.count > 0);
    assert(id != LAYLA_ELEMENT_ID_NONE);
    node_open((Node) {.id = id, .type = LAYLA_NODE_TEXT});
}

void layla_text_element_configure(Layla_TextConfig conf) {
    Node *node = node_from_temp_id(layla_list_last(&state.open_node_stack));
    assert(node->type == LAYLA_NODE_TEXT);
    node->as.text.text = conf.text;
    node->as.text.style = conf.style;
    node->as.text.userdata = conf.userdata;
}

void layla_container_element_open(void) {
    assert(state.open_node_stack.count > 0);
    node_open((Node) {.type = LAYLA_NODE_CONTAINER});
}

void layla_container_element_open_with_id(Layla_ElementID id) {
    assert(state.open_node_stack.count > 0);
    assert(id != LAYLA_ELEMENT_ID_NONE);
    node_open((Node) {.id = id, .type = LAYLA_NODE_CONTAINER});
}

void layla_container_element_configure(Layla_ContainerConfig conf) {
    TempID temp_id = layla_list_last(&state.open_node_stack);
    Node *node = node_from_temp_id(temp_id);
    assert(node->type == LAYLA_NODE_CONTAINER);
    node->as.container.style = conf.style;
    node->as.container.floating = conf.floating;
    node->as.container.custom = conf.custom;

    if (conf.floating.attach_to != LAYLA_ATTACH_TO_NONE) {
        assert(state.open_node_stack.count > 1);
        node->parent = state.open_node_stack.items[state.open_node_stack.count - 2];
        layla_list_append(&state.floating_roots, temp_id);
    }
}

void layla_element_close(void) {
    TempID closed_id = layla_list_pop(&state.open_node_stack);
    Node *closed = node_from_temp_id(closed_id);
    isize start = state.temporary_child_stack.count - closed->children.count;
    isize end = state.temporary_child_stack.count;
    closed->children.offset = state.frame_children.count;
    // Take last IDs from the temp child stack
    // and attach them to contiguous child list
    state.temporary_child_stack.count = start;
    for (isize i = start; i < end; ++i) {
        TempID child_id = state.temporary_child_stack.items[i];
        layla_list_append(&state.frame_children, child_id);
        // Attach parent to child
        Node *child = node_from_temp_id(child_id);
        child->parent = closed_id;
    }
    if (node_is_floating(closed)) return;

    layla_list_append(&state.temporary_child_stack, closed_id);
    if (closed_id > LAYLA_ROOT_TEMP_ID) {
        TempID parent_id = layla_list_last(&state.open_node_stack);
        Node *parent = node_from_temp_id(parent_id);
        parent->children.count++;
    }
}

Layla_ElementIDSlice layla_state_get_hovered_element_ids(void) {
    return (Layla_ElementIDSlice) {
        .items = state.hovered_element_ids.items,
        .count = state.hovered_element_ids.count,
    };
}

b32 layla_state_is_element_hovered(void) {
    if (state.open_node_stack.count == 0) return false;

    TempID current_id = layla_list_last(&state.open_node_stack);
    Layla_ElementID element_id = node_from_temp_id(current_id)->id;
    return layla_state_is_element_hovered_by_id(element_id);
}

b32 layla_state_is_element_hovered_by_id(Layla_ElementID id) {
    if (id == LAYLA_ELEMENT_ID_NONE) return false;

    for (isize i = 0; i < state.hovered_element_ids.count; ++i)
        if (state.hovered_element_ids.items[i] == id) return true;

    return false;
}

Layla_ElementID layla_element_get_open_id(void) {
    assert(state.open_node_stack.count > 0);
    return node_from_temp_id(layla_list_last(&state.open_node_stack))->id;
}

void layla_scroll_offset_set_by_id(Layla_ScrollID id, i32 offset_y) {
    if (id == LAYLA_SCROLL_ID_NONE) return;
    scroll_state_get_by_id(id)->y = offset_y;
}

void layla_scroll_offset_update_by_id(Layla_ScrollID id, i32 delta_y) {
    if (id == LAYLA_SCROLL_ID_NONE || delta_y == 0) return;
    ScrollState *scroll = scroll_state_get_by_id(id);
    i64 offset_y = (i64)scroll->y + (i64)delta_y;
    scroll->y = (i32)CLAMP(offset_y, (i64)INT32_MIN, (i64)INT32_MAX);
}

i32 layla_scroll_offset_get_by_id(Layla_ScrollID id) {
    if (id == LAYLA_SCROLL_ID_NONE) return 0;
    return scroll_state_get_by_id(id)->y;
}

i32 layla_scroll_max_offset_get_by_id(Layla_ScrollID id) {
    if (id == LAYLA_SCROLL_ID_NONE) return 0;
    return scroll_state_get_by_id(id)->max_y;
}

void layla_scroll_offset_update_on_hovered_element(i32 delta_y) {
    TempID current_id = state.hovered_temp_id;
    if (delta_y == 0 || current_id == LAYLA_TEMP_ID_NONE) return;

    for (;;) {
        Node *current = node_from_temp_id(current_id);
        if (node_is_scroll_y(current)) {
            layla_scroll_offset_update_by_id(current->as.container.style.scroll.id, delta_y);
            return;
        }

        if (node_is_floating(current)) break;
        if (current_id == LAYLA_ROOT_TEMP_ID) break;
        current_id = current->parent;
    }
}

static inline void floating_layout(Node *node) {
    Layla_Floating floating = node->as.container.floating;
    Node *attached;
    switch (floating.attach_to) {
        case LAYLA_ATTACH_TO_PARENT: attached = node_from_temp_id(node->parent); break;
        case LAYLA_ATTACH_TO_ROOT:   attached = node_from_temp_id(LAYLA_ROOT_TEMP_ID); break;
        case LAYLA_ATTACH_TO_NONE:   UNREACHABLE("Floating layout requires a floating container");
    }
    container_intrinsic_width(node);
    // Due to floating container not being measured by parent, it needs to measure w/h on its own
    floating_measure_size(node, attached, DIM_X);
    container_fill_width(node);
    container_wrap_text(node);
    container_intrinsic_height(node);
    // Same here
    floating_measure_size(node, attached, DIM_Y);
    container_fill_height(node);
    node->x = attached->x + align_offset(floating.align_x, attached->w, (PaddingSides) {0}, node->w);
    node->y = attached->y + align_offset(floating.align_y, attached->h, (PaddingSides) {0}, node->h); 
    container_positions(node);
    container_commands(node);
}

static inline void floating_measure_size(Node *node, Node *attached, Dimension dim) {
    Layla_SizeStyle size_style = get_size_style(node->as.container.style, dim);
    i32 attached_size = *node_get_size(attached, dim);

    switch (size_style.type) {
        case LAYLA_SIZE_FIXED:
        case LAYLA_SIZE_FIT: break;
        case LAYLA_SIZE_FILL: {
            SizeRange range = get_size_range(size_style);
            i32 minimum_size = MAX(range.min, *node_get_min_size(node, dim));
            *node_get_size(node, dim) = CLAMP(attached_size, minimum_size, range.max);
            break;
        }
        case LAYLA_SIZE_PERCENT: *node_get_size(node, dim) = size_from_percentage(size_style, attached_size);
    }
}

static inline void floating_roots_sort(void) {
    for (isize i = 1; i < state.floating_roots.count; ++i) {
        TempID current_id = state.floating_roots.items[i];
        i32 current_z_index = node_from_temp_id(current_id)->as.container.floating.z_index;
        isize position = i;

        while (position > 0) {
            TempID previous_id = state.floating_roots.items[position - 1];
            i32 previous_z_index = node_from_temp_id(previous_id)->as.container.floating.z_index;
            if (previous_z_index <= current_z_index) break;

            state.floating_roots.items[position] = previous_id;
            position--;
        }

        state.floating_roots.items[position] = current_id;
    }
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
    Layla_ContainerStyle style = node->as.container.style;
    Layla_SizeStyle size_style = get_size_style(style, dim);
    PaddingSides padding = padding_sides_from_container_style(style, dim);
    i32 total_padding = padding.start + padding.end;
    if (size_style.type == LAYLA_SIZE_FIXED) {
        *node_get_size(node, dim) = *node_get_min_size(node, dim) = size_style.as.fixed.value;
        return;
    }

    i32 resolved_size =     0;
    i32 resolved_min_size = 0;

    if (dim == direction_get_main_dimension(style.direction)) {
        resolved_size =     total_padding + get_children_spacing(children, style.spacing);
        resolved_min_size = total_padding + get_children_spacing(children, style.spacing);
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
        resolved_size +=     total_padding;
        resolved_min_size += total_padding;
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
    Layla_ContainerStyle style = node->as.container.style;
    ChildrenIndices children = node->children;
    PaddingSides padding = padding_sides_from_container_style(style, dim);
    i32 total_padding = padding.start + padding.end;

    if (dim == direction_get_main_dimension(style.direction)) {
        i32 spacing = get_children_spacing(children, style.spacing);
        i32 available_size = MAX(*node_get_size(node, dim) - total_padding - spacing, 0);
        for (isize i = 0; i < children.count; ++i) {
            Node *child = node_from_index(children.offset + i);
            if (node_is_percentage(child, dim)) {
                Layla_SizeStyle size_style = get_size_style(child->as.container.style, dim);
                *node_get_size(child, dim) = size_from_percentage(size_style, available_size);
            }
        }

        i32 children_size = 0;
        for (isize i = 0; i < children.count; ++i) {
            children_size += *node_get_size(node_from_index(children.offset + i), dim);
        }

        i32 remaining_size = *node_get_size(node, dim) - children_size - total_padding - spacing;

        i32 fill_count = 0;
        for (isize i = 0; i < children.count; ++i) {
            fill_count += node_is_fill(node_from_index(children.offset + i), dim);
        }

        if (fill_count > 0) {
            Scratch scratch = scratch_begin(&state.tmp);
            Node **fill_items = arena_push(scratch.arena, Node *, fill_count);
            assert(fill_items != NULL && "Layla temporary storage capacity was exceeded");
            List(NodePtr) fill_list = {
                .items = fill_items,
                .capacity = fill_count,
            };

            for (isize i = 0; i < children.count; ++i) {
                Node *child = node_from_index(children.offset + i);
                if (node_is_fill(child, dim)) layla_list_append(&fill_list, child);
            }

            space_distribute(remaining_size, fill_list, dim);
            scratch_end(scratch);
        }
    } else {
        i32 content_size = *node_get_size(node, dim) - total_padding;
        i32 available_size = MAX(content_size, 0);
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
                    Layla_SizeStyle size_style = get_size_style(child->as.container.style, dim);
                    if (size_style.type == LAYLA_SIZE_PERCENT) {
                        *node_get_size(child, dim) = size_from_percentage(size_style, available_size);
                    } else if (size_style.type == LAYLA_SIZE_FILL) {
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
    Layla_ContainerStyle style = node->as.container.style;
    ChildrenIndices children = node->children;
    Dimension main_dim = direction_get_main_dimension(style.direction);
    Dimension cross_dim = dimension_get_other(main_dim);
    PaddingSides main_padding = padding_sides_from_container_style(style, main_dim);
    PaddingSides cross_padding = padding_sides_from_container_style(style, cross_dim);
    PaddingSides vertical_padding = padding_sides_from_container_style(style, DIM_Y);
    i32 children_main_size = get_children_spacing(children, style.spacing);
    i32 content_bottom = node->y + vertical_padding.start;

    for (isize i = 0; i < children.count; ++i) {
        children_main_size += *node_get_size(node_from_index(children.offset + i), main_dim);
    }

    i32 cursor = *node_get_pos(node, main_dim)
                 + align_offset(style.align_children, *node_get_size(node, main_dim), main_padding, children_main_size);

    for (isize i = 0; i < children.count; ++i) {
        Node *child = node_from_index(children.offset + i);
        *node_get_pos(child, main_dim) = cursor;
        *node_get_pos(child, cross_dim) = *node_get_pos(node, cross_dim) + align_offset(
            node_get_align_self(child),
            *node_get_size(node, cross_dim),
            cross_padding,
            *node_get_size(child, cross_dim)
        );

        content_bottom = MAX(content_bottom, child->y + child->h);
        cursor += *node_get_size(child, main_dim) + style.spacing;
    }

    if (node_is_scroll_y(node)) {
        ScrollState *scroll = scroll_state_get_by_id(node->as.container.style.scroll.id);
        i32 content_h = MAX(content_bottom + vertical_padding.end - node->y, 0);
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

static inline void container_commands(Node *node) {
    Layla_ContainerStyle style = node->as.container.style;
    b32 overflow_hidden = style.overflow == LAYLA_OVERFLOW_HIDDEN;

    if (overflow_hidden) {
        layla_list_append(&state.commands,
            ((Layla_Command) {.type = LAYLA_CMD_CLIP_START, .id = node->id, .as.clip_start = {
                .x = node->x, .y = node->y, .w = node->w, .h = node->h
            }})
        );
    }

    if (style.background.is_set) {
        layla_list_append(&state.commands,
            ((Layla_Command) {.type = LAYLA_CMD_RECTANGLE, .id = node->id, .as.rectangle = {
                .x = node->x, .y = node->y, .w = node->w, .h = node->h,
                .color = style.background.color,
            }})
        );
    }

    if (node->as.container.custom != NULL) {
        PaddingSides horizontal_padding = padding_sides_from_container_style(style, DIM_X);
        PaddingSides vertical_padding = padding_sides_from_container_style(style, DIM_Y);
        i32 custom_w = node->w - horizontal_padding.start - horizontal_padding.end;
        i32 custom_h = node->h - vertical_padding.start - vertical_padding.end;
        if (custom_w > 0 && custom_h > 0) {
            layla_list_append(&state.commands, ((Layla_Command) {
                .type = LAYLA_CMD_CUSTOM,
                .id = node->id,
                .as.custom = {
                    .x = node->x + horizontal_padding.start,
                    .y = node->y + vertical_padding.start,
                    .w = custom_w,
                    .h = custom_h,
                    .userdata = node->as.container.custom,
                },
            }));
        }
    }

    ChildrenIndices children = node->children;
    for (isize i = 0; i < children.count; ++i) {
        Node *child = node_from_index(children.offset + i);
        if (child->type == LAYLA_NODE_TEXT) {
            text_process(child, MAX(child->w, 1), true);
        } else {
            container_commands(child);
        }
    }

    Layla_BorderStyle border = style.border;
    for (i32 i = 0; i < border.width; ++i) {
        i32 w = node->w - 2 * i;
        i32 h = node->h - 2 * i;
        if (w < 2 || h < 2) continue;

        layla_list_append(&state.commands, ((Layla_Command) {
            .type = LAYLA_CMD_BORDER,
            .id = node->id,
            .as.border = {
                .x = node->x + i,
                .y = node->y + i,
                .w = w,
                .h = h,
                .color = border.color,
                .userdata = border.userdata,
            },
        }));
    }

    if (overflow_hidden) {
        layla_list_append(&state.commands, ((Layla_Command) {
            .type = LAYLA_CMD_CLIP_END, .id = node->id,
        }));
    }
}

static inline TextMeasurement text_process(Node *node, i32 wrap_width, b32 emit_commands) {
    Layla_TextSlice source = node->as.text.text;
    Layla_TextStyle style = node->as.text.style;
    TextMeasurement measurement = {0};
    if (source.count <= 0) return measurement;

    if (state.text_measure_function == NULL) {
        error_emit(
            LAYLA_ERROR_TEXT_MEASURE_FUNCTION_NOT_SET,
            node->id,
            "Layla text measure function must be set before laying out text"
        );
        return measurement;
    }

    switch (style.wrap_policy) {
        case LAYLA_TEXT_WRAP_WORD: break;
        default: UNREACHABLE("Unknown text wrapping policy");
    }

    isize cursor_byte = 0;
    isize explicit_line_start_byte = 0;
    isize line_start_byte = 0;
    isize line_end_byte = 0;
    i32 line_width = 0;
    b32 line_has_word = false;

    while (cursor_byte < source.count) {
        if (source.items[cursor_byte] == '\n') {
            Layla_TextSlice explicit_line = {
                .items = source.items + explicit_line_start_byte,
                .count = cursor_byte - explicit_line_start_byte,
            };
            measurement.natural_width = MAX(measurement.natural_width, text_slice_measure(node->id, explicit_line));

            Layla_TextSlice line = {
                .items = source.items + line_start_byte,
                .count = cursor_byte - line_start_byte,
            };
            line_width = text_slice_measure(node->id, line);

            if (emit_commands) {
                i32 line_x = node->x + align_offset(style.alignment, node->w, ((PaddingSides) {0}), line_width);
                append_text_command(node, line_start_byte, cursor_byte, line_x, node->y + measurement.line_count);
            }

            measurement.line_count++;
            cursor_byte++;
            explicit_line_start_byte = cursor_byte;
            line_start_byte = cursor_byte;
            line_end_byte = cursor_byte;
            line_width = 0;
            line_has_word = false;
            continue;
        }

        if (source.items[cursor_byte] == ' ') {
            while (cursor_byte < source.count && source.items[cursor_byte] == ' ') cursor_byte++;
            continue;
        }

        isize word_start_byte = cursor_byte;
        while (cursor_byte < source.count &&
               source.items[cursor_byte] != ' ' &&
               source.items[cursor_byte] != '\n') {
            cursor_byte++;
        }

        Layla_TextSlice word = {
            .items = source.items + word_start_byte,
            .count = cursor_byte - word_start_byte,
        };
        i32 word_width = text_slice_measure(node->id, word);
        measurement.minimum_width = MAX(measurement.minimum_width, word_width);

        Layla_TextSlice line_with_word = {
            .items = source.items + line_start_byte,
            .count = cursor_byte - line_start_byte,
        };
        i32 width_with_word = text_slice_measure(node->id, line_with_word);
        b32 word_overflows_line = wrap_width > 0 && line_has_word && width_with_word > wrap_width;
        if (word_overflows_line) {
            if (emit_commands) {
                i32 line_x = node->x + align_offset(style.alignment, node->w, ((PaddingSides) {0}), line_width);
                append_text_command(node, line_start_byte, line_end_byte, line_x, node->y + measurement.line_count);
            }

            measurement.line_count++;
            line_start_byte = word_start_byte;
            line_width = word_width;
        } else {
            line_width = width_with_word;
        }

        line_end_byte = cursor_byte;
        line_has_word = true;
    }

    Layla_TextSlice explicit_line = {
        .items = source.items + explicit_line_start_byte,
        .count = source.count - explicit_line_start_byte,
    };
    measurement.natural_width = MAX(measurement.natural_width, text_slice_measure(node->id, explicit_line));

    Layla_TextSlice line = {
        .items = source.items + line_start_byte,
        .count = source.count - line_start_byte,
    };
    line_width = text_slice_measure(node->id, line);

    if (emit_commands) {
        i32 line_x = node->x + align_offset(style.alignment, node->w, ((PaddingSides) {0}), line_width);
        append_text_command(node, line_start_byte, source.count, line_x, node->y + measurement.line_count);
    }
    measurement.line_count++;

    return measurement;
}

static inline i32 text_slice_measure(Layla_ElementID id, Layla_TextSlice text) {
    i32 width = state.text_measure_function(text, state.text_measure_userdata);
    if (width < 0) {
        error_emit(
            LAYLA_ERROR_TEXT_MEASURE_RETURNED_NEGATIVE_WIDTH,
            id, "Layla text measure function returned a negative width"
        );
        return 0;
    }
    return width;
}

static inline void append_text_command(Node *node, isize line_start_byte, isize line_end_byte, i32 line_x, i32 line_y) {
    Layla_TextSlice source = node->as.text.text;
    Layla_TextStyle style = node->as.text.style;
    layla_list_append(&state.commands,
        ((Layla_Command) {.type = LAYLA_CMD_TEXT, .id = node->id, .as.text = {
            .x = line_x,
            .y = line_y,
            .slice = {
                .items = source.items + line_start_byte,
                .count = line_end_byte - line_start_byte,
            },
            .color = style.color,
            .userdata = node->as.text.userdata,
        }})
    );
}

static inline void hover_test(void) {
    Node *root = node_from_temp_id(LAYLA_ROOT_TEMP_ID);
    Layla_Rectangle screen = rect_from_node(root);
    state.hovered_temp_id = LAYLA_TEMP_ID_NONE;
    state.hovered_element_ids.count = 0;

    for (isize i = state.floating_roots.count; i > 0; --i) {
        TempID floating_root = state.floating_roots.items[i - 1];
        if (node_hit_test(node_from_temp_id(floating_root), screen, state.cursor.x, state.cursor.y)) return;
    }

    node_hit_test(root, screen, state.cursor.x, state.cursor.y);
}

static inline b32 node_hit_test(Node *node, Layla_Rectangle parent_clip, i32 x, i32 y) {
    if (!rect_contains_point(x, y, parent_clip)) return false;

    Layla_Rectangle node_rect = rect_from_node(node);
    Layla_Rectangle node_clip = rect_intersect(parent_clip, node_rect);
    b32 node_contains = rect_contains_point(x, y, node_clip);

    b32 overflow_hidden = node->type == LAYLA_NODE_CONTAINER
        && node->as.container.style.overflow == LAYLA_OVERFLOW_HIDDEN;

    if (overflow_hidden && !node_contains) return false;

    b32 found = false;
    if (node_contains) {
        state.hovered_temp_id = (TempID)(node - state.nodes.items);
        if (node->id != LAYLA_ELEMENT_ID_NONE) layla_list_append(&state.hovered_element_ids, node->id);
        found = true;
    }

    Layla_Rectangle child_clip = overflow_hidden ? node_clip : parent_clip;
    ChildrenIndices children = node->children;
    for (isize i = 0; i < children.count; ++i) {
        Node *child = node_from_index(children.offset + i);
        if (node_hit_test(child, child_clip, x, y)) found = true;
    }

    return found;
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
    layla_list_append(&state.nodes, node);
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
        case LAYLA_NODE_CONTAINER: return node->as.container.style.align_self;
        case LAYLA_NODE_TEXT: return LAYLA_ALIGN_START;
    }
    UNREACHABLE("It must always match against alignment");
}

static inline i32 align_offset(Layla_Alignment align, i32 parent_size, PaddingSides padding, i32 child_size) {
    i32 parent_inner = parent_size - padding.start - padding.end;
    i32 remaining = parent_inner - child_size;
    switch (align) {
        case LAYLA_ALIGN_START:  return padding.start;
        case LAYLA_ALIGN_CENTER: return padding.start + remaining / 2;
        case LAYLA_ALIGN_END:    return padding.start + remaining;
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
        case LAYLA_SIZE_PERCENT: return (SizeRange) {size.as.percent.min, size.as.percent.max};
        case LAYLA_SIZE_FIT:   return (SizeRange) {size.as.fit.min, size.as.fit.max};
        case LAYLA_SIZE_FILL:  return (SizeRange) {size.as.fill.min, size.as.fill.max};
    }
    UNREACHABLE("Unknown size type");
}

static inline PaddingSides padding_sides_from_container_style(Layla_ContainerStyle style, Dimension dim) {
    Layla_Padding padding = style.padding;
    i32 border_width = style.border.width;
    if (dim == DIM_X) {
        return (PaddingSides) {
            .start = padding.left + border_width,
            .end = padding.right + border_width,
        };
    }

    return (PaddingSides) {
        .start = padding.top + border_width,
        .end = padding.bottom + border_width,
    };
}

static inline i32 get_children_spacing(ChildrenIndices children, i32 spacing) {
    return MAX(children.count - 1, 0) * spacing;
}

static inline b32 node_is_fill(Node *node, Dimension dim) {
    switch (node->type) {
        case LAYLA_NODE_TEXT: return dim == DIM_X;
        case LAYLA_NODE_CONTAINER:
            return get_size_style(node->as.container.style, dim).type == LAYLA_SIZE_FILL;
    }
    return false;
}

static inline b32 node_is_percentage(Node *node, Dimension dim) {
    switch (node->type) {
        case LAYLA_NODE_TEXT: return false;
        case LAYLA_NODE_CONTAINER:
            return get_size_style(node->as.container.style, dim).type == LAYLA_SIZE_PERCENT;
    }
    return false;
}

static inline i32 size_from_percentage(Layla_SizeStyle size, i32 available_size) {
    assert(size.type == LAYLA_SIZE_PERCENT && "function is only for percentage size");
    SizeRange range = get_size_range(size);
    i32 resolved = MAX(available_size, 0) * size.as.percent.value;
    return CLAMP(resolved, range.min, range.max);
}

static inline i32 node_get_fill_max(Node *node, Dimension dim) {
    switch (node->type) {
        case LAYLA_NODE_TEXT: return *node_get_size(node, dim);
        case LAYLA_NODE_CONTAINER: {
            Layla_SizeStyle size = get_size_style(node->as.container.style, dim);
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
            Layla_SizeStyle size = get_size_style(node->as.container.style, dim);
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

//TODO: improve performance
static inline ScrollState *scroll_state_get_by_id(Layla_ScrollID id) {
    for (isize i = 0; i < state.scroll_states.count; ++i) {
        ScrollState *scroll_state = &state.scroll_states.items[i];
        if (scroll_state->id == id) return scroll_state;
    }

    layla_list_append(&state.scroll_states, ((ScrollState) {.id = id}));
    return &layla_list_last(&state.scroll_states);
}

static inline b32 node_is_scroll_y(Node *node) {
    switch (node->type) {
        case LAYLA_NODE_CONTAINER: return node->as.container.style.scroll.axis == LAYLA_SCROLL_Y;
        case LAYLA_NODE_TEXT: return false;
    }
    return false;
}

static inline b32 node_is_floating(Node *node) {
    return node->type == LAYLA_NODE_CONTAINER && node->as.container.floating.attach_to != LAYLA_ATTACH_TO_NONE;
}
