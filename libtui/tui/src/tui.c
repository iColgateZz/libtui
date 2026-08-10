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

    Tui_TextInputBindings *text_input = &bindings->text_input;
    text_input->delete_left = binding_resolve(text_input->delete_left, TUI_BINDING_KEY(BRENDA_TERM_KEY_BACKSPACE, 0));
    text_input->delete_right = binding_resolve(text_input->delete_right, TUI_BINDING_KEY(BRENDA_TERM_KEY_DELETE, 0));
    text_input->cursor_left = binding_resolve(text_input->cursor_left, TUI_BINDING_KEY(BRENDA_TERM_KEY_LEFT, 0));
    text_input->cursor_right = binding_resolve(text_input->cursor_right, TUI_BINDING_KEY(BRENDA_TERM_KEY_RIGHT, 0));
    text_input->cursor_up = binding_resolve(text_input->cursor_up, TUI_BINDING_KEY(BRENDA_TERM_KEY_UP, 0));
    text_input->cursor_down = binding_resolve(text_input->cursor_down, TUI_BINDING_KEY(BRENDA_TERM_KEY_DOWN, 0));
    text_input->cursor_home = binding_resolve(text_input->cursor_home, TUI_BINDING_KEY(BRENDA_TERM_KEY_HOME, 0));
    text_input->cursor_end = binding_resolve(text_input->cursor_end, TUI_BINDING_KEY(BRENDA_TERM_KEY_END, 0));
    text_input->submit = binding_resolve(text_input->submit, TUI_BINDING_KEY(BRENDA_TERM_KEY_ENTER, 0));

    brenda_terminal_init(config.terminal);
    brenda_terminal_set_fps(config.fps == 0 ? 60 : config.fps);
    layla_state_set_text_measure_function(text_measure, NULL);
}

void tui_deinit(void) {
    brenda_terminal_deinit();
    hash_map_free(&state.interaction_records);
    list_free(state.focus_order);
}

Brenda_EventSlice tui_frame_begin(void) {
    brenda_frame_begin();
    state.events = brenda_events_get();
    interactions_begin(state.events);
    layla_state_set_screen_dimensions(brenda_terminal_get_width(), brenda_terminal_get_height());
    layla_layout_begin();
    return state.events;
}

void tui_frame_end(void) {
    Layla_CommandSlice commands = layla_layout_end();
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
    return layla_state_is_element_hovered_by_id(id);
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
    if (config.id == LAYLA_ELEMENT_ID_NONE) layla_container_element_open();
    else layla_container_element_open_with_id(config.id);

    Layla_ElementID id = layla_state_get_open_element_id();
    if (config.style.scroll != LAYLA_SCROLL_NONE)
        config.flags |= TUI_ELEMENT_HOVERABLE | TUI_ELEMENT_ACCEPTS_SCROLL;
    tui_element_register(id, (Tui_ElementConfig) {.flags = config.flags});
    layla_container_element_configure((Layla_ContainerConfig) {
        .style = config.style,
        .floating = config.floating,
        .custom = config.custom,
    });
}

void tui_text_draw(Tui_TextConfig config) {
    if (config.id == LAYLA_ELEMENT_ID_NONE) layla_text_element_open();
    else layla_text_element_open_with_id(config.id);

    Layla_ElementID id = layla_state_get_open_element_id();
    tui_element_register(id, (Tui_ElementConfig) {.flags = config.flags});
    layla_text_element_configure((Layla_TextConfig) {
        .text = config.text,
        .style = config.style,
        .userdata = config.userdata,
    });
    layla_element_close();
}

b32 tui_button_draw(Tui_ButtonConfig config) {
    if (config.id == LAYLA_ELEMENT_ID_NONE) layla_container_element_open();
    else layla_container_element_open_with_id(config.id);

    Layla_ElementID id = layla_state_get_open_element_id();
    u8 flags = TUI_ELEMENT_HOVERABLE | TUI_ELEMENT_CLICKABLE | TUI_ELEMENT_FOCUSABLE;
    if (config.disabled) flags |= TUI_ELEMENT_DISABLED;
    tui_element_register(id, (Tui_ElementConfig) {.flags = flags});

    if      (tui_element_is_pressed(id)) config.style.background = config.pressed_background;
    else if (tui_element_is_hovered(id)) config.style.background = config.hovered_background;
    else if (tui_element_is_focused(id)) config.style.background = config.focused_background;

    layla_container_element_configure((Layla_ContainerConfig) {.style = config.style});
    Tui_Text(.text = config.text, .style = config.text_style);
    layla_element_close();
    return tui_element_is_clicked(id);
}

Tui_TextInputResult tui_text_input_draw(Tui_TextInputConfig config) {
    Tui_TextInputResult result = {0};
    config.state->count = CLAMP(config.state->count, 0, config.state->capacity);
    config.state->cursor = CLAMP(config.state->cursor, 0, config.state->count);

    if (config.id == LAYLA_ELEMENT_ID_NONE) layla_container_element_open();
    else layla_container_element_open_with_id(config.id);

    Layla_ElementID id = layla_state_get_open_element_id();
    u8 flags = TUI_ELEMENT_HOVERABLE | TUI_ELEMENT_CLICKABLE | TUI_ELEMENT_FOCUSABLE | ELEMENT_TEXT_INPUT;
    if (config.disabled) flags |= TUI_ELEMENT_DISABLED;
    tui_element_register(id, (Tui_ElementConfig) {.flags = flags});

    b32 focused = tui_element_is_focused(id);
    if (focused) {
        config.style.background = config.focused_background;
        Layla_ElementData data = layla_state_get_element_data(id);
        i32 horizontal_inset = config.style.padding.left + config.style.padding.right
            + config.style.border.width * 2;
        i32 wrap_width = data.found ? MAX(data.rectangle.w - horizontal_inset, 1) : 1;
        text_input_events_handle(config.state, &result, wrap_width, config.text_style.wrap_policy);
    }

    layla_container_element_configure((Layla_ContainerConfig) {.style = config.style});
    Layla_TextSlice text = {.items = config.state->items, .count = config.state->count};
    Layla_TextStyle style = config.text_style;
    if (config.state->count == 0 && focused) text = LAYLA_TEXT_SLICE(" ");
    if (config.state->count == 0 && !focused) {
        text = config.placeholder;
        style = config.placeholder_style;
    }
    Tui_Text(.text = text, .style = style, .userdata = focused ? config.state : NULL);

    layla_element_close();
    return result;
}

void tui_text_input_state_set_text(Tui_TextInputState *input, Layla_TextSlice text) {
    isize count = MIN(text.count, input->capacity);
    if (count < text.count) {
        while (count > 0 && ((u8)text.items[count] & 0xc0) == 0x80) count--;
    }
    memcpy(input->items, text.items, count);
    input->count = count;
    input->cursor = count;
}

static inline InteractionRecord *interaction_record_get(Layla_ElementID id) {
    InteractionRecord *record = NULL;
    hash_map_get(&state.interaction_records, id, &record);
    return record != NULL && record->generation == state.generation ? record : NULL;
}

static inline Layla_ElementID interaction_target_get(u8 required_flags) {
    Layla_ElementIDSlice hovered = layla_state_get_hovered_element_ids();

    for (isize i = hovered.count; i > 0; --i) {
        Layla_ElementID id = hovered.items[i - 1];
        while (id != LAYLA_ELEMENT_ID_NONE) {
            InteractionRecord *record = interaction_record_get(id);
            if (record != NULL) {
                if (record->config.flags & TUI_ELEMENT_DISABLED) break;
                if ((record->config.flags & required_flags) == required_flags) return id;
            }

            Layla_ElementData data = layla_state_get_element_data(id);
            if (!data.found) break;
            id = data.parent_id;
        }
    }

    return LAYLA_ELEMENT_ID_NONE;
}

static inline void interactions_begin(Brenda_EventSlice events) {
    state.clicked_id = LAYLA_ELEMENT_ID_NONE;

    Layla_CursorState cursor = layla_state_get_cursor_state();
    b32 cursor_is_down = cursor.interaction_state == LAYLA_CURSOR_PRESSED_THIS_FRAME
        || cursor.interaction_state == LAYLA_CURSOR_PRESSED;
    b32 cursor_was_set = false;

    for (isize i = 0; i < events.count; ++i) {
        Brenda_Event event = events.items[i];

        //TODO: set_cursor_state does hover test every time it is called
        //      why not call it once after going through all events?
        switch (event.type) {
            case BRENDA_EVENT_MOUSE_LEFT: {
                cursor.x = event.as.mouse.x;
                cursor.y = event.as.mouse.y;
                cursor_is_down = event.as.mouse.pressed;
                layla_state_set_cursor_state(cursor.x, cursor.y, cursor_is_down);
                cursor_was_set = true;

                Layla_ElementID target = interaction_target_get(TUI_ELEMENT_CLICKABLE);
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
                break;
            }
            case BRENDA_EVENT_MOUSE_RIGHT:
            case BRENDA_EVENT_MOUSE_MIDDLE:
            case BRENDA_EVENT_MOUSE_MOVE:
                cursor.x = event.as.mouse.x;
                cursor.y = event.as.mouse.y;
                layla_state_set_cursor_state(cursor.x, cursor.y, cursor_is_down);
                cursor_was_set = true;
                break;
            case BRENDA_EVENT_MOUSE_DRAG:
                cursor.x = event.as.mouse.x;
                cursor.y = event.as.mouse.y;
                cursor_is_down = true;
                layla_state_set_cursor_state(cursor.x, cursor.y, cursor_is_down);
                cursor_was_set = true;
                break;
            case BRENDA_EVENT_SCROLL_UP:
            case BRENDA_EVENT_SCROLL_DOWN: {
                cursor.x = event.as.mouse.x;
                cursor.y = event.as.mouse.y;
                layla_state_set_cursor_state(cursor.x, cursor.y, cursor_is_down);
                cursor_was_set = true;

                Layla_ElementID target = interaction_target_get(TUI_ELEMENT_ACCEPTS_SCROLL);
                Layla_ElementData data = layla_state_get_element_data(target);
                if (data.found && (data.flags & LAYLA_ELEMENT_SCROLL_Y)) {
                    i32 delta_y = event.type == BRENDA_EVENT_SCROLL_UP ? -1 : 1;
                    layla_state_update_scroll_offset(target, delta_y);
                }
                break;
            }
            default:
                if (binding_matches_event(state.config.bindings.focus_clear, event)) {
                    state.focused_id = LAYLA_ELEMENT_ID_NONE;
                } else if (binding_matches_event(state.config.bindings.focus_next, event)) {
                    focus_move(1);
                } else if (binding_matches_event(state.config.bindings.focus_previous, event)) {
                    focus_move(-1);
                } else if ((binding_matches_event(state.config.bindings.activate, event)
                            || binding_matches_event(state.config.bindings.activate_alternate, event))
                           && state.focused_id != LAYLA_ELEMENT_ID_NONE)
                {
                    InteractionRecord *record = interaction_record_get(state.focused_id);
                    if (record != NULL && (record->config.flags & TUI_ELEMENT_CLICKABLE))
                        state.clicked_id = state.focused_id;
                }
                break;
        }
    }

    if (!cursor_was_set) layla_state_set_cursor_state(cursor.x, cursor.y, cursor_is_down);

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

                Layla_ElementData text_data = layla_state_get_element_data(command.id);
                InteractionRecord *parent = interaction_record_get(text_data.parent_id);
                if (text.userdata != NULL && parent != NULL && (parent->config.flags & ELEMENT_TEXT_INPUT)) {
                    text_input_cursor_draw(text, text.userdata);
                }
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

static inline void text_input_cursor_draw(Layla_CommandText text, Tui_TextInputState *input) {
    Brenda_TextEffect effect = {
        .color = color_from_layla(text.color),
        .flags = BRENDA_TEXT_EFFECT_UNDERLINE,
    };

    if (input->count == 0) {
        brenda_text_draw(text.x, text.y, (byte *)" ", 1, effect);
        return;
    }

    byte *cursor = input->items + input->cursor;
    byte *line_start = text.slice.items;
    byte *line_end = line_start + text.slice.count;
    if (cursor < line_start || cursor > line_end) return;
    if (cursor == line_end && input->cursor < input->count) return;

    i32 cursor_x = text.x + brenda_text_measure_width(line_start, cursor - line_start);
    if (input->cursor == input->count) {
        brenda_text_draw(cursor_x, text.y, " ", 1, effect);
        return;
    }

    isize next = input->cursor + 1;
    while (next < input->count && ((u8)input->items[next] & 0xc0) == 0x80) next++;
    brenda_text_draw(cursor_x, text.y, cursor, next - input->cursor, effect);
}

static inline void text_input_events_handle(
    Tui_TextInputState *input,
    Tui_TextInputResult *result,
    i32 wrap_width,
    Layla_TextWrapPolicy wrap_policy
) {
    Tui_TextInputBindings bindings = state.config.bindings.text_input;

    for (isize i = 0; i < state.events.count; ++i) {
        Brenda_Event event = state.events.items[i];

        if (binding_matches_event(state.config.bindings.focus_next, event)
            || binding_matches_event(state.config.bindings.focus_previous, event)
            || binding_matches_event(state.config.bindings.focus_clear, event)) {
            continue;
        }

        if (binding_matches_event(bindings.delete_left, event)) {
            if (input->cursor == 0) continue;
            isize previous = utf8_previous_byte(input);
            memmove(input->items + previous, input->items + input->cursor, input->count - input->cursor);
            input->count -= input->cursor - previous;
            input->cursor = previous;
            result->changed = true;
        } else if (binding_matches_event(bindings.delete_right, event)) {
            if (input->cursor == input->count) continue;
            isize next = utf8_next_byte(input);
            memmove(input->items + input->cursor, input->items + next, input->count - next);
            input->count -= next - input->cursor;
            result->changed = true;
        } else if (binding_matches_event(bindings.cursor_left, event)) {
            input->cursor = utf8_previous_byte(input);
        } else if (binding_matches_event(bindings.cursor_right, event)) {
            input->cursor = utf8_next_byte(input);
        } else if (binding_matches_event(bindings.cursor_up, event)) {
            text_input_cursor_move_vertical(input, wrap_width, wrap_policy, -1);
        } else if (binding_matches_event(bindings.cursor_down, event)) {
            text_input_cursor_move_vertical(input, wrap_width, wrap_policy, 1);
        } else if (binding_matches_event(bindings.cursor_home, event)) {
            input->cursor = 0;
        } else if (binding_matches_event(bindings.cursor_end, event)) {
            input->cursor = input->count;
        } else if (binding_matches_event(bindings.submit, event)) {
            result->submitted = true;
        } else if (event.type == BRENDA_EVENT_UTF8
                   && !(event.modifiers & (BRENDA_MODIFIER_CTRL | BRENDA_MODIFIER_ALT))) {
            if (input->count + event.as.utf8.length > input->capacity) continue;
            memmove(
                input->items + input->cursor + event.as.utf8.length,
                input->items + input->cursor,
                input->count - input->cursor
            );
            memcpy(input->items + input->cursor, event.as.utf8.bytes, event.as.utf8.length);
            input->count += event.as.utf8.length;
            input->cursor += event.as.utf8.length;
            result->changed = true;
        }
    }
}

static inline void text_input_cursor_move_vertical(
    Tui_TextInputState *input,
    i32 wrap_width,
    Layla_TextWrapPolicy wrap_policy,
    i32 direction
) {
    isize previous_line_start = -1;
    isize current_line_start = 0;
    isize current_line_end = 0;

    while (current_line_start <= input->count) {
        current_line_end = text_input_line_end(input, current_line_start, wrap_width, wrap_policy);
        b32 cursor_is_on_line = input->cursor < current_line_end
            || (input->cursor == current_line_end
                && (current_line_end == input->count || input->items[current_line_end] == '\n'));
        if (cursor_is_on_line) break;

        previous_line_start = current_line_start;
        current_line_start = current_line_end;
        if (current_line_start < input->count && input->items[current_line_start] == '\n') current_line_start++;
    }

    isize target_line_start;
    if (direction < 0) {
        if (previous_line_start < 0) return;
        target_line_start = previous_line_start;
    } else {
        if (current_line_end == input->count) return;
        target_line_start = current_line_end;
        if (input->items[target_line_start] == '\n') target_line_start++;
    }

    isize target_line_end = text_input_line_end(input, target_line_start, wrap_width, wrap_policy);
    i32 target_column = brenda_text_measure_width(
        input->items + current_line_start,
        input->cursor - current_line_start
    );

    isize target_cursor = target_line_start;
    while (target_cursor < target_line_end) {
        isize next_cursor = target_cursor + 1;
        while (next_cursor < target_line_end && ((u8)input->items[next_cursor] & 0xc0) == 0x80) next_cursor++;
        i32 next_column = brenda_text_measure_width(
            input->items + target_line_start,
            next_cursor - target_line_start
        );
        if (next_column > target_column) break;
        target_cursor = next_cursor;
    }

    input->cursor = target_cursor;
}

static inline isize text_input_line_end(
    Tui_TextInputState *input,
    isize line_start,
    i32 wrap_width,
    Layla_TextWrapPolicy wrap_policy
) {
    if (wrap_policy == LAYLA_TEXT_WRAP_WORD) {
        isize cursor = line_start;
        isize line_end = line_start;
        b32 line_has_word = false;

        while (cursor < input->count && input->items[cursor] != '\n') {
            if (input->items[cursor] == ' ') {
                while (cursor < input->count && input->items[cursor] == ' ') cursor++;
                continue;
            }

            while (cursor < input->count && input->items[cursor] != ' ' && input->items[cursor] != '\n') cursor++;
            i32 width = brenda_text_measure_width(input->items + line_start, cursor - line_start);
            if (line_has_word && width > wrap_width) return line_end;

            line_end = cursor;
            line_has_word = true;
        }

        return cursor;
    }

    isize cursor = line_start;
    isize line_end = line_start;

    while (cursor < input->count && input->items[cursor] != '\n') {
        isize next_cursor = cursor + 1;
        while (next_cursor < input->count && ((u8)input->items[next_cursor] & 0xc0) == 0x80) next_cursor++;

        i32 width = brenda_text_measure_width(input->items + line_start, next_cursor - line_start);
        if (line_end > line_start && width > wrap_width) break;

        cursor = next_cursor;
        line_end = cursor;
    }

    return line_end;
}

static inline isize utf8_previous_byte(Tui_TextInputState *input) {
    if (input->cursor == 0) return 0;
    isize cursor = input->cursor - 1;
    while (cursor > 0 && ((u8)input->items[cursor] & 0xc0) == 0x80) cursor--;
    return cursor;
}

static inline isize utf8_next_byte(Tui_TextInputState *input) {
    if (input->cursor >= input->count) return input->count;
    isize cursor = input->cursor + 1;
    while (cursor < input->count && ((u8)input->items[cursor] & 0xc0) == 0x80) cursor++;
    return cursor;
}
