#include "tui_internal.h"

static inline u64 element_id_hash(Layla_ElementID id) { return id; }
static inline b32 element_id_equal(Layla_ElementID a, Layla_ElementID b) { return a == b; }
static inline Brenda_Color color_from_layla(Layla_Color color) {
    return (Brenda_Color) {.r = color.r, .g = color.g, .b = color.b, .is_set = color.is_set};
}
static inline i32 text_measure(Layla_TextSlice text, void *userdata) {
    UNUSED(userdata);
    return brenda_text_measure_width(text.items, text.count);
}

static State state = {
    .interaction_records = {
        .key_hash = element_id_hash,
        .key_equal = element_id_equal,
    },
};

static inline Tui_Binding binding_resolve(Tui_Binding binding, Tui_Binding default_binding) {
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

void tui_init(Tui_Config config) {
    state.config = config;
    Tui_Bindings *bindings = &state.config.bindings;
    bindings->focus_next = binding_resolve(bindings->focus_next, TUI_BINDING_KEY(BRENDA_TERM_KEY_TAB, 0));
    bindings->focus_previous = binding_resolve(bindings->focus_previous, TUI_BINDING_KEY(BRENDA_TERM_KEY_TAB, BRENDA_MODIFIER_SHIFT));
    bindings->focus_clear = binding_resolve(bindings->focus_clear, TUI_BINDING_KEY(BRENDA_TERM_KEY_ESCAPE, 0));
    bindings->activate = binding_resolve(bindings->activate, TUI_BINDING_KEY(BRENDA_TERM_KEY_ENTER, 0));
    bindings->activate_alternate = binding_resolve(bindings->activate_alternate, TUI_BINDING_CHAR(' ', 0));

    brenda_terminal_init(config.terminal);
    brenda_terminal_set_fps(config.fps == 0 ? 60 : config.fps);
    layla_set_text_measure_function(text_measure, NULL);
}

void tui_deinit(void) {
    brenda_terminal_deinit();
    hash_map_free(&state.interaction_records);
    list_free(state.focus_order);
    list_free(state.unhandled_events);
}

Tui_EventSlice tui_frame_begin(void) {
    brenda_frame_begin();
    Tui_EventSlice unhandled_events = events_route(brenda_events_get());
    layla_set_screen_dimensions(brenda_terminal_get_width(), brenda_terminal_get_height());
    layla_begin_layout();
    return unhandled_events;
}

void tui_frame_end(void) {
    Layla_CommandSlice commands = layla_end_layout();
    interactions_end();
    commands_draw(commands);
    brenda_frame_end();
}

void tui_element_register(Layla_ElementID id, Tui_ElementConfig config) {
    hash_map_insert(&state.interaction_records, id, ((InteractionRecord) {
        .config = config,
        .generation = state.generation,
    }));
    state.registered_count++;

    if (config.flags & TUI_ELEMENT_FOCUSABLE) list_append(&state.focus_order, id);
}

b32 tui_element_is_hovered(Layla_ElementID id) {
    InteractionRecord *record = interaction_record_get(id);
    if (record != NULL && (!(record->config.flags & TUI_ELEMENT_HOVERABLE)
        || (record->config.flags & TUI_ELEMENT_DISABLED))) return false;
    return layla_is_element_hovered(id);
}

b32 tui_element_is_pressed(Layla_ElementID id) {
    InteractionRecord *record = interaction_record_get(id);
    return (record == NULL || !(record->config.flags & TUI_ELEMENT_DISABLED)) && state.pressed_id == id;
}

b32 tui_element_is_clicked(Layla_ElementID id) {
    InteractionRecord *record = interaction_record_get(id);
    return (record == NULL || !(record->config.flags & TUI_ELEMENT_DISABLED)) && state.clicked_id == id;
}

b32 tui_element_is_focused(Layla_ElementID id) {
    InteractionRecord *record = interaction_record_get(id);
    return (record == NULL || !(record->config.flags & TUI_ELEMENT_DISABLED)) && state.focused_id == id;
}

void tui_element_focus(Layla_ElementID id) {
    if (id == LAYLA_ELEMENT_ID_NONE) {
        state.focused_id = LAYLA_ELEMENT_ID_NONE;
        return;
    }

    InteractionRecord *record = interaction_record_get(id);
    if (record != NULL && (record->config.flags & TUI_ELEMENT_FOCUSABLE)
        && !(record->config.flags & TUI_ELEMENT_DISABLED)) {
        state.focused_id = id;
    }
}

Layla_ElementID tui_element_get_focused_id(void) { return state.focused_id; }

void tui_div_open(Tui_DivConfig config) {
    if (config.id == LAYLA_ELEMENT_ID_NONE) layla_open_container_element();
    else layla_open_container_element_with_id(config.id);

    Layla_ElementID id = layla_get_open_element_id();
    if (config.style.scroll != LAYLA_SCROLL_NONE)
        config.flags |= TUI_ELEMENT_HOVERABLE | TUI_ELEMENT_ACCEPTS_SCROLL;
    tui_element_register(id, (Tui_ElementConfig) {.flags = config.flags});
    layla_configure_container_element((Layla_ContainerConfig) {
        .style = config.style,
        .floating = config.floating,
        .custom = config.custom,
    });
}

void tui_text_draw(Tui_TextConfig config) {
    if (config.id == LAYLA_ELEMENT_ID_NONE) layla_open_text_element();
    else layla_open_text_element_with_id(config.id);

    Layla_ElementID id = layla_get_open_element_id();
    tui_element_register(id, (Tui_ElementConfig) {.flags = config.flags});
    layla_configure_text_element((Layla_TextConfig) {
        .text = config.text,
        .style = config.style,
        .userdata = config.userdata,
    });
    layla_close_element();
}

b32 tui_button_draw(Tui_ButtonConfig config) {
    if (config.id == LAYLA_ELEMENT_ID_NONE) layla_open_container_element();
    else layla_open_container_element_with_id(config.id);

    Layla_ElementID id = layla_get_open_element_id();
    u8 flags = TUI_ELEMENT_HOVERABLE | TUI_ELEMENT_CLICKABLE | TUI_ELEMENT_FOCUSABLE;
    if (config.disabled) flags |= TUI_ELEMENT_DISABLED;
    tui_element_register(id, (Tui_ElementConfig) {.flags = flags});

    if      (tui_element_is_pressed(id)) config.style.background = config.pressed_background;
    else if (tui_element_is_hovered(id)) config.style.background = config.hovered_background;
    else if (tui_element_is_focused(id)) config.style.background = config.focused_background;

    layla_configure_container_element((Layla_ContainerConfig) {.style = config.style});
    Tui_Text(.text = config.text, .style = config.text_style);
    layla_close_element();
    return tui_element_is_clicked(id);
}

static inline InteractionRecord *interaction_record_get(Layla_ElementID id) {
    InteractionRecord *record = NULL;
    hash_map_get(&state.interaction_records, id, &record);
    return record != NULL && record->generation == state.generation ? record : NULL;
}

static inline Layla_ElementID interaction_target_get(u8 required_flags) {
    Layla_ElementIDSlice hovered = layla_get_hovered_element_ids();

    for (isize i = hovered.count; i > 0; --i) {
        Layla_ElementID id = hovered.items[i - 1];
        while (id != LAYLA_ELEMENT_ID_NONE) {
            InteractionRecord *record = interaction_record_get(id);
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

static inline Tui_EventSlice events_route(Brenda_EventSlice events) {
    state.clicked_id = LAYLA_ELEMENT_ID_NONE;
    list_clear(&state.unhandled_events);

    Layla_CursorState cursor = layla_get_cursor_state();
    b32 cursor_is_down = cursor.interaction_state == LAYLA_CURSOR_PRESSED_THIS_FRAME
        || cursor.interaction_state == LAYLA_CURSOR_PRESSED;
    b32 cursor_was_set = false;

    for (isize i = 0; i < events.count; ++i) {
        Brenda_Event event = events.items[i];

        switch (event.type) {
            case BRENDA_EVENT_MOUSE_LEFT: {
                cursor.x = event.as.mouse.x;
                cursor.y = event.as.mouse.y;
                cursor_is_down = event.as.mouse.pressed;
                layla_set_cursor_state(cursor.x, cursor.y, cursor_is_down);
                cursor_was_set = true;

                Layla_ElementID target = interaction_target_get(TUI_ELEMENT_CLICKABLE);
                Layla_ElementID pressed = state.pressed_id;
                if (event.as.mouse.pressed) {
                    state.pressed_id = target;
                    InteractionRecord *record = interaction_record_get(target);
                    state.focused_id = record != NULL && (record->config.flags & TUI_ELEMENT_FOCUSABLE)
                        ? target
                        : LAYLA_ELEMENT_ID_NONE;
                } else {
                    if (target != LAYLA_ELEMENT_ID_NONE && target == state.pressed_id) state.clicked_id = target;
                    state.pressed_id = LAYLA_ELEMENT_ID_NONE;
                }
                if (target == LAYLA_ELEMENT_ID_NONE && pressed == LAYLA_ELEMENT_ID_NONE) {
                    list_append(&state.unhandled_events, ((Tui_Event) {
                        .target_id = interaction_target_get(TUI_ELEMENT_HOVERABLE),
                        .event = event,
                    }));
                }
                break;
            }
            case BRENDA_EVENT_MOUSE_RIGHT:
            case BRENDA_EVENT_MOUSE_MIDDLE:
            case BRENDA_EVENT_MOUSE_MOVE:
                cursor.x = event.as.mouse.x;
                cursor.y = event.as.mouse.y;
                layla_set_cursor_state(cursor.x, cursor.y, cursor_is_down);
                cursor_was_set = true;
                list_append(&state.unhandled_events, ((Tui_Event) {
                    .target_id = interaction_target_get(TUI_ELEMENT_HOVERABLE),
                    .event = event,
                }));
                break;
            case BRENDA_EVENT_MOUSE_DRAG:
                cursor.x = event.as.mouse.x;
                cursor.y = event.as.mouse.y;
                cursor_is_down = true;
                layla_set_cursor_state(cursor.x, cursor.y, cursor_is_down);
                cursor_was_set = true;
                if (state.pressed_id == LAYLA_ELEMENT_ID_NONE) {
                    list_append(&state.unhandled_events, ((Tui_Event) {
                        .target_id = interaction_target_get(TUI_ELEMENT_HOVERABLE),
                        .event = event,
                    }));
                }
                break;
            case BRENDA_EVENT_SCROLL_UP:
            case BRENDA_EVENT_SCROLL_DOWN: {
                cursor.x = event.as.mouse.x;
                cursor.y = event.as.mouse.y;
                layla_set_cursor_state(cursor.x, cursor.y, cursor_is_down);
                cursor_was_set = true;

                Layla_ElementID target = interaction_target_get(TUI_ELEMENT_ACCEPTS_SCROLL);
                Layla_ElementData data = layla_get_element_data(target);
                if (data.found && (data.flags & LAYLA_ELEMENT_SCROLL_Y)) {
                    i32 delta_y = event.type == BRENDA_EVENT_SCROLL_UP ? -1 : 1;
                    layla_update_scroll_offset(target, delta_y);
                } else {
                    list_append(&state.unhandled_events, ((Tui_Event) {
                        .target_id = interaction_target_get(TUI_ELEMENT_HOVERABLE),
                        .event = event,
                    }));
                }
                break;
            }
            default: {
                if (binding_matches_event(state.config.bindings.focus_clear, event)) {
                    state.focused_id = LAYLA_ELEMENT_ID_NONE;
                } else if (binding_matches_event(state.config.bindings.focus_next, event)) {
                    focus_move(1);
                } else if (binding_matches_event(state.config.bindings.focus_previous, event)) {
                    focus_move(-1);
                } else {
                    InteractionRecord *record = interaction_record_get(state.focused_id);
                    if (record != NULL
                        && (record->config.flags & TUI_ELEMENT_CLICKABLE)
                        && (binding_matches_event(state.config.bindings.activate, event)
                            || binding_matches_event(state.config.bindings.activate_alternate, event))) {
                        state.clicked_id = state.focused_id;
                    } else {
                        Layla_ElementID target_id = event.type == BRENDA_EVENT_TERM_KEY
                            || event.type == BRENDA_EVENT_UTF8
                            ? state.focused_id
                            : LAYLA_ELEMENT_ID_NONE;
                        list_append(&state.unhandled_events, ((Tui_Event) {
                            .target_id = target_id,
                            .event = event,
                        }));
                    }
                }
                break;
            }
        }
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

    return (Tui_EventSlice) {
        .items = state.unhandled_events.items,
        .count = state.unhandled_events.count,
    };
}

static inline void interactions_end(void) {
    InteractionRecord *focused = interaction_record_get(state.focused_id);
    if (focused == NULL || (focused->config.flags & TUI_ELEMENT_DISABLED))
        state.focused_id = LAYLA_ELEMENT_ID_NONE;

    InteractionRecord *pressed = interaction_record_get(state.pressed_id);
    if (pressed == NULL || (pressed->config.flags & TUI_ELEMENT_DISABLED))
        state.pressed_id = LAYLA_ELEMENT_ID_NONE;
}

static inline void focus_move(i32 direction) {
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
        InteractionRecord *record = interaction_record_get(id);
        if (record != NULL && !(record->config.flags & TUI_ELEMENT_DISABLED)) {
            state.focused_id = id;
            return;
        }
    }

    state.focused_id = LAYLA_ELEMENT_ID_NONE;
}

static inline void commands_draw(Layla_CommandSlice commands) {
    for (isize i = 0; i < commands.count; ++i) {
        Layla_Command command = commands.items[i];
        if (state.config.command_handler != NULL
            && state.config.command_handler(command, state.config.command_handler_userdata)) {
            continue;
        }

        switch (command.type) {
            case LAYLA_CMD_RECTANGLE: {
                Layla_CommandRectangle rectangle = command.as.rectangle;
                brenda_rectangle_fill(
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
                brenda_text_draw(text.x, text.y, text.slice.items, text.slice.count, effect);
                break;
            }
            case LAYLA_CMD_BORDER: {
                Layla_CommandBorder border = command.as.border;
                Brenda_Rectangle rectangle = {.x = border.x, .y = border.y, .w = border.w, .h = border.h};
                Brenda_TextEffect effect = {
                    .color = color_from_layla(border.color),
                };
                brenda_box_draw(rectangle, effect);
                break;
            }
            case LAYLA_CMD_CLIP_START: {
                Layla_CommandClipStart clip = command.as.clip_start;
                brenda_clip_push(clip.x, clip.y, clip.w, clip.h);
                break;
            }
            case LAYLA_CMD_CLIP_END: brenda_clip_pop(); break;
            case LAYLA_CMD_CUSTOM: break;
        }
    }
}
