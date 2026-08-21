#include "tui_internal.h"

static State state = {
    .interaction_records = {
        .key_hash = hash_element_id,
        .key_equal = equal_element_ids,
    },
    .drag_positions = {
        .key_hash = hash_element_id,
        .key_equal = equal_element_ids,
    },
};

// Program lifetime

void tui_init(Tui_Config config) {
    state.config = config;
    Tui_Bindings *bindings = &state.config.bindings;
    bindings->focus_next = resolve_binding(bindings->focus_next, TUI_BINDING_KEY(BRENDA_TERM_KEY_TAB, 0));
    bindings->focus_previous = resolve_binding(bindings->focus_previous, TUI_BINDING_KEY(BRENDA_TERM_KEY_TAB, BRENDA_MODIFIER_SHIFT));
    bindings->focus_clear = resolve_binding(bindings->focus_clear, TUI_BINDING_KEY(BRENDA_TERM_KEY_ESCAPE, 0));
    bindings->activate = resolve_binding(bindings->activate, TUI_BINDING_KEY(BRENDA_TERM_KEY_ENTER, 0));
    bindings->activate_alternate = resolve_binding(bindings->activate_alternate, TUI_BINDING_CHAR(' ', 0));

    brenda_init_terminal(config.terminal);
    brenda_set_terminal_fps(config.fps == 0 ? 60 : config.fps);
    layla_set_text_measure_function(measure_text, NULL);
}

void tui_deinit(void) {
    brenda_deinit_terminal();
    hash_map_free(&state.interaction_records);
    hash_map_free(&state.drag_positions);
    list_free(state.focus_order);
    list_free(state.routed_events);
    list_free(state.unhandled_events);
}

// Frame loop

void tui_begin_frame(void) {
    brenda_begin_frame();
    route_events(brenda_get_events());
    layla_set_screen_dimensions(brenda_get_terminal_width(), brenda_get_terminal_height());
    layla_begin_layout();
}

void tui_end_frame(void) {
    Layla_CommandSlice commands = layla_end_layout();
    draw_commands(commands);
    brenda_end_frame();
}

// Interaction state and event routing

void tui_register_element(Layla_ElementID id, Tui_ElementConfig config) {
    hash_map_insert(&state.interaction_records, id, ((InteractionRecord) {
        .config = config,
        .generation = state.generation,
    }));
    state.registered_count++;

    if (config.flags & TUI_ELEMENT_FOCUSABLE) list_append(&state.focus_order, id);

    //TODO: I really dislike the fact that positioning happens here
    DragPosition *position = get_drag_position_by_id(id);
    if (position != NULL && (config.flags & TUI_ELEMENT_DRAGGABLE)) {
        layla_set_element_position(id, position->x, position->y);
    }
}

b32 tui_is_element_hovered(Layla_ElementID id) {
    InteractionRecord *record = get_interaction_record_by_id(id);
    if (record != NULL && (!(record->config.flags & TUI_ELEMENT_HOVERABLE)
        || (record->config.flags & TUI_ELEMENT_DISABLED))) return false;
    return layla_is_element_hovered(id);
}

b32 tui_is_element_pressed(Layla_ElementID id) {
    InteractionRecord *record = get_interaction_record_by_id(id);
    return (record == NULL || !(record->config.flags & TUI_ELEMENT_DISABLED)) && state.pressed_id == id;
}

b32 tui_is_element_clicked(Layla_ElementID id) {
    InteractionRecord *record = get_interaction_record_by_id(id);
    return (record == NULL || !(record->config.flags & TUI_ELEMENT_DISABLED)) && state.clicked_id == id;
}

b32 tui_is_element_focused(Layla_ElementID id) {
    InteractionRecord *record = get_interaction_record_by_id(id);
    return (record == NULL || !(record->config.flags & TUI_ELEMENT_DISABLED)) && state.focused_id == id;
}

void tui_focus_element(Layla_ElementID id) {
    if (id == LAYLA_ELEMENT_ID_NONE) {
        state.focused_id = LAYLA_ELEMENT_ID_NONE;
        return;
    }

    InteractionRecord *record = get_interaction_record_by_id(id);
    if (record != NULL && (record->config.flags & TUI_ELEMENT_FOCUSABLE)
        && !(record->config.flags & TUI_ELEMENT_DISABLED)) {
        state.focused_id = id;
    }
}

Layla_ElementID tui_get_focused_element_id(void) { return state.focused_id; }

Tui_DragState tui_get_drag_state(Layla_ElementID id) {
    if (state.active_drag.state.element_id == id) return state.active_drag.state;

    DragPosition *position = get_drag_position_by_id(id);
    if (position == NULL) return (Tui_DragState) {0};
    return (Tui_DragState) {.element_id = id, .start_x = position->x, .start_y = position->y};
}

void tui_consume_element_specific_events(Layla_ElementID id, Tui_EventHandler handler, void *userdata) {
    if (handler == NULL) return;

    for (isize i = 0; i < state.routed_events.count; ++i) {
        RoutedEvent *routed = &state.routed_events.items[i];
        if (!routed->consumed && routed->event.target_id == id) {
            routed->consumed = handler(routed->event, userdata);
        }
    }
}

Tui_EventSlice tui_get_unhandled_events(void) {
    list_clear(&state.unhandled_events);
    for (isize i = 0; i < state.routed_events.count; ++i) {
        RoutedEvent routed = state.routed_events.items[i];
        if (!routed.consumed) list_append(&state.unhandled_events, routed.event);
    }

    return (Tui_EventSlice) {
        .items = state.unhandled_events.items,
        .count = state.unhandled_events.count
    };
}

static inline u64 hash_element_id(Layla_ElementID id) { return id; }
static inline b32 equal_element_ids(Layla_ElementID a, Layla_ElementID b) { return a == b; }

static inline Tui_Binding resolve_binding(Tui_Binding binding, Tui_Binding default_binding) {
    return binding.type == TUI_BINDING_USE_DEFAULT ? default_binding : binding;
}

static inline b32 binding_matches_event(Tui_Binding binding, Brenda_Event event) {
    if (binding.modifiers != event.modifiers) return false;

    switch (binding.type) {
        case TUI_BINDING_TERM_KEY:
            return event.type == BRENDA_EVENT_TERM_KEY && event.as.term_key == binding.as.term_key;
        case TUI_BINDING_CHARACTER:
            return event.type == BRENDA_EVENT_UTF8
                && event.as.utf8.length == 1
                && event.as.utf8.bytes[0] == binding.as.character;
        case TUI_BINDING_USE_DEFAULT:
        case TUI_BINDING_DISABLED: return false;
    }

    return false;
}

static inline InteractionRecord *get_interaction_record_by_id(Layla_ElementID id) {
    InteractionRecord *record = NULL;
    hash_map_get(&state.interaction_records, id, &record);
    return record != NULL && record->generation == state.generation ? record : NULL;
}

static inline DragPosition *get_drag_position_by_id(Layla_ElementID id) {
    DragPosition *position = NULL;
    hash_map_get(&state.drag_positions, id, &position);
    return position;
}

static inline Layla_ElementID get_interaction_target_by_flags(u8 required_flags) {
    Layla_ElementIDSlice hovered = layla_get_hovered_element_ids();

    for (isize i = hovered.count; i > 0; --i) {
        Layla_ElementID id = hovered.items[i - 1];
        while (id != LAYLA_ELEMENT_ID_NONE) {
            InteractionRecord *record = get_interaction_record_by_id(id);
            if (record != NULL) {
                if (record->config.flags & TUI_ELEMENT_DISABLED) break;
                if ((record->config.flags & required_flags) == required_flags) return id;
            }

            Layla_ElementData data = layla_get_element_data(id);
            if (!data.found) break;
            id = data.parent_id;
        }
    }

    return LAYLA_ELEMENT_ID_NONE;
}

static inline void route_events(Brenda_EventSlice events) {
    state.clicked_id = LAYLA_ELEMENT_ID_NONE;
    list_clear(&state.routed_events);
    list_clear(&state.unhandled_events);

    if (state.active_drag.state.interaction_state == TUI_DRAG_STARTED) {
        state.active_drag.state.interaction_state = TUI_DRAGGING;
    } else if (state.active_drag.state.interaction_state == TUI_DRAG_RELEASED) {
        state.active_drag = (ActiveDrag) {0};
    }

    Layla_CursorState cursor = layla_get_cursor_state();
    b32 cursor_is_down = cursor.interaction_state == LAYLA_CURSOR_PRESSED_THIS_FRAME
        || cursor.interaction_state == LAYLA_CURSOR_PRESSED;
    b32 cursor_was_set = false;

    for (isize i = 0; i < events.count; ++i) {
        Brenda_Event event = events.items[i];
        RoutedEvent routed = {.event.event = event};

        b32 event_has_cursor_position = event.type == BRENDA_EVENT_MOUSE_LEFT
            || event.type == BRENDA_EVENT_MOUSE_RIGHT
            || event.type == BRENDA_EVENT_MOUSE_MIDDLE
            || event.type == BRENDA_EVENT_MOUSE_MOVE
            || event.type == BRENDA_EVENT_MOUSE_DRAG
            || event.type == BRENDA_EVENT_SCROLL_UP
            || event.type == BRENDA_EVENT_SCROLL_DOWN;
        if (event_has_cursor_position) {
            cursor.x = event.as.mouse.x;
            cursor.y = event.as.mouse.y;
            if (event.type == BRENDA_EVENT_MOUSE_LEFT) cursor_is_down = event.as.mouse.pressed;
            if (event.type == BRENDA_EVENT_MOUSE_DRAG) cursor_is_down = true;
            layla_set_cursor_state(cursor.x, cursor.y, cursor_is_down);
            cursor_was_set = true;
            routed.event.target_id = get_interaction_target_by_flags(TUI_ELEMENT_HOVERABLE);
        }

        switch (event.type) {
            case BRENDA_EVENT_MOUSE_LEFT: {
                Layla_ElementID click_target = get_interaction_target_by_flags(TUI_ELEMENT_CLICKABLE);
                Layla_ElementID focus_target = get_interaction_target_by_flags(TUI_ELEMENT_HOVERABLE | TUI_ELEMENT_FOCUSABLE);
                Layla_ElementID previous_pressed_id = state.pressed_id;
                Layla_ElementID previous_drag_id = state.active_drag.state.element_id;
                Layla_ElementID drag_target = LAYLA_ELEMENT_ID_NONE;

                if (event.as.mouse.pressed) {
                    state.pressed_id = click_target;
                    drag_target = get_interaction_target_by_flags(TUI_ELEMENT_DRAGGABLE);
                    if (drag_target != LAYLA_ELEMENT_ID_NONE) {
                        DragPosition *position = get_drag_position_by_id(drag_target);
                        if (position == NULL) {
                            Layla_ElementData data = layla_get_element_data(drag_target);
                            state.active_drag.state.start_x = data.rectangle.x;
                            state.active_drag.state.start_y = data.rectangle.y;
                        } else {
                            state.active_drag.state.start_x = position->x;
                            state.active_drag.state.start_y = position->y;
                        }

                        state.active_drag.state.element_id = drag_target;
                        state.active_drag.state.interaction_state = TUI_DRAG_STARTED;
                        state.active_drag.state.delta_x = 0;
                        state.active_drag.state.delta_y = 0;
                        state.active_drag.cursor_start_x = cursor.x;
                        state.active_drag.cursor_start_y = cursor.y;
                    } else {
                        state.active_drag = (ActiveDrag) {0};
                    }

                    state.focused_id = focus_target;
                } else {
                    Tui_DragState *drag = &state.active_drag.state;
                    if (drag->element_id != LAYLA_ELEMENT_ID_NONE) {
                        drag->delta_x = cursor.x - state.active_drag.cursor_start_x;
                        drag->delta_y = cursor.y - state.active_drag.cursor_start_y;
                        if (get_drag_position_by_id(drag->element_id) != NULL
                            || drag->delta_x != 0 || drag->delta_y != 0) {
                            hash_map_insert(&state.drag_positions, drag->element_id, ((DragPosition) {
                                .x = drag->start_x + drag->delta_x,
                                .y = drag->start_y + drag->delta_y,
                            }));
                        }
                        drag->interaction_state = TUI_DRAG_RELEASED;
                    }

                    b32 element_was_dragged = drag->element_id != LAYLA_ELEMENT_ID_NONE
                        && (drag->delta_x != 0 || drag->delta_y != 0);
                    if (!element_was_dragged && click_target != LAYLA_ELEMENT_ID_NONE
                        && click_target == state.pressed_id) state.clicked_id = click_target;
                    state.pressed_id = LAYLA_ELEMENT_ID_NONE;
                }

                routed.consumed = click_target != LAYLA_ELEMENT_ID_NONE
                    || previous_pressed_id != LAYLA_ELEMENT_ID_NONE;
                routed.consumed |= drag_target != LAYLA_ELEMENT_ID_NONE
                    || previous_drag_id != LAYLA_ELEMENT_ID_NONE;
                break;
            }
            case BRENDA_EVENT_MOUSE_RIGHT:
            case BRENDA_EVENT_MOUSE_MIDDLE:
            case BRENDA_EVENT_MOUSE_MOVE: break;
            case BRENDA_EVENT_MOUSE_DRAG: {
                Tui_DragState *drag = &state.active_drag.state;
                if (drag->element_id != LAYLA_ELEMENT_ID_NONE) {
                    drag->delta_x = cursor.x - state.active_drag.cursor_start_x;
                    drag->delta_y = cursor.y - state.active_drag.cursor_start_y;
                    hash_map_insert(&state.drag_positions, drag->element_id, ((DragPosition) {
                        .x = drag->start_x + drag->delta_x,
                        .y = drag->start_y + drag->delta_y,
                    }));
                    drag->interaction_state = TUI_DRAGGING;
                    routed.consumed = true;
                }
                break;
            }
            case BRENDA_EVENT_SCROLL_UP:
            case BRENDA_EVENT_SCROLL_DOWN: {
                Layla_ElementID target = get_interaction_target_by_flags(TUI_ELEMENT_ACCEPTS_SCROLL);
                Layla_ElementData data = layla_get_element_data(target);
                if (data.found && (data.flags & LAYLA_ELEMENT_SCROLL_Y)) {
                    i32 delta_y = event.type == BRENDA_EVENT_SCROLL_UP ? -1 : 1;
                    layla_update_scroll_offset(target, delta_y);
                    routed.consumed = true;
                }
                break;
            }
            default: {
                if (binding_matches_event(state.config.bindings.focus_clear, event)) {
                    state.focused_id = LAYLA_ELEMENT_ID_NONE;
                    routed.consumed = true;
                } else if (binding_matches_event(state.config.bindings.focus_next, event)) {
                    move_focus(1);
                    routed.consumed = true;
                } else if (binding_matches_event(state.config.bindings.focus_previous, event)) {
                    move_focus(-1);
                    routed.consumed = true;
                } else {
                    InteractionRecord *record = get_interaction_record_by_id(state.focused_id);
                    b32 focused_element_is_enabled = record != NULL && !(record->config.flags & TUI_ELEMENT_DISABLED);
                    if (focused_element_is_enabled
                        && (record->config.flags & TUI_ELEMENT_CLICKABLE)
                        && (binding_matches_event(state.config.bindings.activate, event)
                            || binding_matches_event(state.config.bindings.activate_alternate, event))) {
                        state.clicked_id = state.focused_id;
                        routed.consumed = true;
                    } else {
                        b32 keyboard_event = event.type == BRENDA_EVENT_TERM_KEY
                            || event.type == BRENDA_EVENT_UTF8;
                        if (keyboard_event && focused_element_is_enabled) routed.event.target_id = state.focused_id;
                    }
                }
                break;
            }
        }

        list_append(&state.routed_events, routed);
    }

    if (!cursor_was_set) layla_set_cursor_state(cursor.x, cursor.y, cursor_is_down);

    u32 next_generation = state.generation + 1;
    b32 no_live_records = state.registered_count == 0 && state.interaction_records.count > 0;
    b32 too_many_stale_records = state.registered_count > 0
        && state.interaction_records.count >= state.registered_count * 2;
    if (next_generation == 0 || no_live_records || too_many_stale_records) {
        hash_map_clear(&state.interaction_records);
        if (next_generation == 0) next_generation = 1;
    }

    state.generation = next_generation;
    state.registered_count = 0;
    list_clear(&state.focus_order);
}

static inline void move_focus(i32 direction) {
    if (state.focus_order.count == 0) {
        state.focused_id = LAYLA_ELEMENT_ID_NONE;
        return;
    }

    isize index = direction > 0 ? -1 : 0;
    for (isize i = 0; i < state.focus_order.count; ++i) {
        if (state.focus_order.items[i] == state.focused_id) {
            index = i;
            break;
        }
    }

    for (isize attempts = 0; attempts < state.focus_order.count; ++attempts) {
        index += direction;
        if (index < 0) index = state.focus_order.count - 1;
        if (index >= state.focus_order.count) index = 0;

        Layla_ElementID id = state.focus_order.items[index];
        InteractionRecord *record = get_interaction_record_by_id(id);
        if (record != NULL && !(record->config.flags & TUI_ELEMENT_DISABLED)) {
            state.focused_id = id;
            return;
        }
    }

    state.focused_id = LAYLA_ELEMENT_ID_NONE;
}

// Layla and Brenda adapter

static inline Brenda_Color color_from_layla(Layla_Color color) {
    return (Brenda_Color) {.r = color.r, .g = color.g, .b = color.b, .is_set = color.is_set};
}

static inline i32 measure_text(Layla_TextSlice text, void *userdata) {
    UNUSED(userdata);
    return brenda_measure_text_width(text.items, text.count);
}

static inline void draw_commands(Layla_CommandSlice commands) {
    for (isize i = 0; i < commands.count; ++i) {
        Layla_Command command = commands.items[i];

        if (command.type == LAYLA_CMD_CUSTOM) {
            InteractionRecord *record = get_interaction_record_by_id(command.id);
            if (record != NULL && (record->config.flags & ELEMENT_INTERNAL_CUSTOM_COMMAND)) {
                CustomCommand *custom = command.as.custom.userdata;
                switch (custom->type) {
                    case CUSTOM_COMMAND_TEXT_INPUT_CURSOR:
                        brenda_apply_text_effect(command.as.custom.x, command.as.custom.y, custom->as.text_input_cursor);
                        break;
                }
                continue;
            }
        }

        Tui_CommandHandler handler = state.config.command_handler;
        void *userdata = state.config.command_handler_userdata;
        if (handler != NULL && handler(command, userdata)) {
            continue;
        }

        switch (command.type) {
            case LAYLA_CMD_RECTANGLE: {
                Layla_CommandRectangle rectangle = command.as.rectangle;
                brenda_fill_rectangle(
                    (Brenda_Rectangle) {.x = rectangle.x, .y = rectangle.y, .w = rectangle.w, .h = rectangle.h},
                    color_from_layla(rectangle.color)
                );
                break;
            }
            case LAYLA_CMD_TEXT: {
                Layla_CommandText text = command.as.text;
                Brenda_TextEffect effect = {
                    .color = color_from_layla(text.color),
                };
                brenda_draw_text(text.x, text.y, text.slice.items, text.slice.count, effect);
                break;
            }
            case LAYLA_CMD_BORDER: {
                Layla_CommandBorder border = command.as.border;
                Brenda_Rectangle rectangle = {.x = border.x, .y = border.y, .w = border.w, .h = border.h};
                Brenda_TextEffect effect = {
                    .color = color_from_layla(border.color),
                };
                brenda_draw_box(rectangle, effect);
                break;
            }
            case LAYLA_CMD_CLIP_START: {
                Layla_CommandClipStart clip = command.as.clip_start;
                brenda_push_clip(clip.x, clip.y, clip.w, clip.h);
                break;
            }
            case LAYLA_CMD_CLIP_END: brenda_pop_clip(); break;
            case LAYLA_CMD_CUSTOM: break;
        }
    }
}

// Widgets

void tui_open_div(Tui_DivConfig config) {
    if (config.id == LAYLA_ELEMENT_ID_NONE) layla_open_container_element();
    else layla_open_container_element_with_id(config.id);

    Layla_ElementID id = layla_get_open_element_id();
    if (config.style.scroll != LAYLA_SCROLL_NONE)
        config.flags |= TUI_ELEMENT_HOVERABLE | TUI_ELEMENT_ACCEPTS_SCROLL;
    if (config.floating.attach_to.type != LAYLA_ATTACH_TO_NONE && config.floating.draggable)
        config.flags |= TUI_ELEMENT_DRAGGABLE;
    tui_register_element(id, (Tui_ElementConfig) {.flags = config.flags});
    layla_configure_container_element((Layla_ContainerConfig) {
        .style = config.style,
        .floating = {
            .attach_to = config.floating.attach_to,
            .attach_point = {
                .parent = config.floating.attach_point.parent,
                .element = config.floating.attach_point.element,
            },
            .cursor_capture_mode = config.floating.cursor_capture_mode,
            .z_index = config.floating.z_index,
        },
        .custom = config.custom,
    });
}

void tui_draw_text(Tui_TextConfig config) {
    if (config.id == LAYLA_ELEMENT_ID_NONE) layla_open_text_element();
    else layla_open_text_element_with_id(config.id);

    Layla_ElementID id = layla_get_open_element_id();
    tui_register_element(id, (Tui_ElementConfig) {.flags = config.flags});
    layla_configure_text_element((Layla_TextConfig) {
        .text = config.text,
        .style = config.style,
        .marker = config.marker,
        .userdata = config.userdata,
    });
    layla_close_element();
}

b32 tui_draw_button(Tui_ButtonConfig config) {
    if (config.id == LAYLA_ELEMENT_ID_NONE) layla_open_container_element();
    else layla_open_container_element_with_id(config.id);

    Layla_ElementID id = layla_get_open_element_id();
    u8 flags = TUI_ELEMENT_HOVERABLE | TUI_ELEMENT_CLICKABLE | TUI_ELEMENT_FOCUSABLE;
    if (config.disabled) flags |= TUI_ELEMENT_DISABLED;
    tui_register_element(id, (Tui_ElementConfig) {.flags = flags});

    if      (tui_is_element_pressed(id)) config.style.background = config.pressed_background;
    else if (tui_is_element_hovered(id)) config.style.background = config.hovered_background;
    else if (tui_is_element_focused(id)) config.style.background = config.focused_background;

    layla_configure_container_element((Layla_ContainerConfig) {.style = config.style});
    Tui_Text(.text = config.text, .style = config.text_style);
    layla_close_element();
    return tui_is_element_clicked(id);
}

b32 tui_draw_text_input(Tui_TextInputConfig config) {
    Tui_TextInputState *input = config.state;
    input->cursor = CLAMP(input->cursor, 0, input->count);

    if (config.id == LAYLA_ELEMENT_ID_NONE) layla_open_container_element();
    else layla_open_container_element_with_id(config.id);

    Layla_ElementID id = layla_get_open_element_id();
    u8 flags = TUI_ELEMENT_HOVERABLE | TUI_ELEMENT_FOCUSABLE;
    if (config.disabled) flags |= TUI_ELEMENT_DISABLED;
    tui_register_element(id, (Tui_ElementConfig) {.flags = flags});

    b32 is_focused = tui_is_element_focused(id);
    if (is_focused && config.focused_background.is_set) {
        config.style.background = config.focused_background;
    }
    layla_configure_container_element((Layla_ContainerConfig) {.style = config.style});

    TextInputEventContext event_context = {.state = input};
    if (is_focused) {
        tui_consume_element_specific_events(id, text_input_handle_event, &event_context);

        state.custom_commands.text_input_cursor = (CustomCommand) {
            .type = CUSTOM_COMMAND_TEXT_INPUT_CURSOR,
            .as.text_input_cursor = {.flags = BRENDA_TEXT_EFFECT_UNDERLINE},
        };
    }

    config.text_style.wrap_policy = LAYLA_TEXT_WRAP_CHARACTER;
    Tui_Text(
        .text = input->count == 0
            ? (Layla_TextSlice) {.items = " ", .count = 1}
            : (Layla_TextSlice) {.items = input->items, .count = input->count},
        .style = config.text_style,
        .flags = is_focused ? ELEMENT_INTERNAL_CUSTOM_COMMAND : 0,
        .marker = is_focused
            ? (Layla_TextMarker) {
                .byte_offset = input->cursor,
                .userdata = &state.custom_commands.text_input_cursor,
            }
            : (Layla_TextMarker) {0},
    );

    layla_close_element();
    return event_context.changed;
}

static inline b32 text_input_handle_event(Tui_Event event, void *userdata) {
    TextInputEventContext *context = userdata;
    Tui_TextInputState *input = context->state;
    Brenda_Event brenda_event = event.event;

    if (brenda_event.type == BRENDA_EVENT_MOUSE_LEFT) return true;

    if (brenda_event.type == BRENDA_EVENT_UTF8) {
        if (brenda_event.modifiers & (BRENDA_MODIFIER_CTRL | BRENDA_MODIFIER_ALT)) return false;
        isize byte_count = brenda_event.as.utf8.length;
        if (byte_count <= 0 || input->count + byte_count > input->capacity) return true;

        memmove(
            input->items + input->cursor + byte_count,
            input->items + input->cursor,
            input->count - input->cursor
        );
        memcpy(input->items + input->cursor, brenda_event.as.utf8.bytes, byte_count);
        input->cursor += byte_count;
        input->count += byte_count;
        context->changed = true;
        return true;
    }

    if (brenda_event.type != BRENDA_EVENT_TERM_KEY) return false;

    switch (brenda_event.as.term_key) {
        case BRENDA_TERM_KEY_LEFT:
            input->cursor -= brenda_distance_to_codepoint_boundary(
                input->items, input->count, input->cursor, BRENDA_UTF8_DIRECTION_BACKWARD);
            return true;
        case BRENDA_TERM_KEY_RIGHT:
            input->cursor += brenda_distance_to_codepoint_boundary(
                input->items, input->count, input->cursor, BRENDA_UTF8_DIRECTION_FORWARD);
            return true;
        case BRENDA_TERM_KEY_HOME: input->cursor = 0; return true;
        case BRENDA_TERM_KEY_END: input->cursor = input->count; return true;
        case BRENDA_TERM_KEY_BACKSPACE:
            if (input->cursor > 0) {
                isize previous = input->cursor
                    - brenda_distance_to_codepoint_boundary(
                        input->items, input->count, input->cursor, BRENDA_UTF8_DIRECTION_BACKWARD);
                memmove(input->items + previous, input->items + input->cursor, input->count - input->cursor);
                input->count -= input->cursor - previous;
                input->cursor = previous;
                context->changed = true;
            }
            return true;
        case BRENDA_TERM_KEY_DELETE:
            if (input->cursor < input->count) {
                isize next = input->cursor
                    + brenda_distance_to_codepoint_boundary(
                        input->items, input->count, input->cursor, BRENDA_UTF8_DIRECTION_FORWARD);
                memmove(input->items + input->cursor, input->items + next, input->count - next);
                input->count -= next - input->cursor;
                context->changed = true;
            }
            return true;
        case BRENDA_TERM_KEY_ENTER:
            if (input->count < input->capacity) {
                memmove(input->items + input->cursor + 1, input->items + input->cursor, input->count - input->cursor);
                input->items[input->cursor++] = '\n';
                input->count++;
                context->changed = true;
            }
            return true;
        default: return false;
    }
}
