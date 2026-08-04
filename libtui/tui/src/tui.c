#include "tui_internal.h"

static inline u64 element_id_hash(Layla_ElementID id) { return id; }
static inline b32 element_id_equal(Layla_ElementID a, Layla_ElementID b) { return a == b; }
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

void tui_init(Tui_Config config) {
    state.config = config;
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
    u8 flags = TUI_ELEMENT_HOVERABLE | TUI_ELEMENT_CLICKABLE | TUI_ELEMENT_FOCUSABLE;
    if (config.disabled) flags |= TUI_ELEMENT_DISABLED;
    tui_element_register(id, (Tui_ElementConfig) {.flags = flags});

    b32 focused = tui_element_is_focused(id);
    if (focused) {
        config.style.background = config.focused_background;
        text_input_events_handle(config.state, &result);
        state.text_input_cursor_id = id;
        state.text_input_cursor_text = (Layla_TextSlice) {
            .items = config.state->items,
            .count = config.state->count,
        };
        state.text_input_cursor_byte = config.state->cursor;
    }

    layla_container_element_configure((Layla_ContainerConfig) {.style = config.style});
    Layla_TextSlice text = {.items = config.state->items, .count = config.state->count};
    Layla_TextStyle style = config.text_style;
    if (config.state->count == 0 && focused) text = LAYLA_TEXT_SLICE(" ");
    if (config.state->count == 0 && !focused) {
        text = config.placeholder;
        style = config.placeholder_style;
    }
    Tui_Text(.text = text, .style = style);

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
    state.text_input_cursor_id = LAYLA_ELEMENT_ID_NONE;

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
                if (event.type == BRENDA_EVENT_TERM_KEY && event.as.term_key == BRENDA_TERM_KEY_TAB
                    && !(event.modifiers & (BRENDA_MODIFIER_CTRL | BRENDA_MODIFIER_ALT))) {
                    focus_move(event.modifiers & BRENDA_MODIFIER_SHIFT ? -1 : 1);
                } else if (event_is_activation(event) && state.focused_id != LAYLA_ELEMENT_ID_NONE) {
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

static inline b32 event_is_activation(Brenda_Event event) {
    if (event.modifiers & (BRENDA_MODIFIER_CTRL | BRENDA_MODIFIER_ALT)) return false;
    if (event.type == BRENDA_EVENT_TERM_KEY) return event.as.term_key == BRENDA_TERM_KEY_ENTER;
    return event.type == BRENDA_EVENT_UTF8 && event.as.utf8.length == 1 && event.as.utf8.bytes[0] == ' ';
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
                    (Brenda_RGB) {.r = rectangle.color.r, .g = rectangle.color.g, .b = rectangle.color.b}
                );
                break;
            }
            case LAYLA_CMD_TEXT: {
                Layla_CommandText text = command.as.text;
                Brenda_TextEffect effect = {
                    .color = {.r = text.color.r, .g = text.color.g, .b = text.color.b},
                };
                brenda_text_draw(text.x, text.y, text.slice.items, text.slice.count, effect);

                Layla_ElementData text_data = layla_state_get_element_data(command.id);
                if (text_data.parent_id == state.text_input_cursor_id) {
                    text_input_cursor_draw(text, state.text_input_cursor_text, state.text_input_cursor_byte);
                }
                break;
            }
            case LAYLA_CMD_BORDER: {
                Layla_CommandBorder border = command.as.border;
                Brenda_Rectangle rectangle = {.x = border.x, .y = border.y, .w = border.w, .h = border.h};
                Brenda_TextEffect effect = {
                    .color = {.r = border.color.r, .g = border.color.g, .b = border.color.b},
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

static inline void text_input_cursor_draw(Layla_CommandText text, Layla_TextSlice input, isize cursor_byte) {
    Brenda_TextEffect effect = {
        .color = {.r = text.color.r, .g = text.color.g, .b = text.color.b},
        .flags = BRENDA_TEXT_EFFECT_UNDERLINE,
    };

    if (input.count == 0) {
        brenda_text_draw(text.x, text.y, (byte *)" ", 1, effect);
        return;
    }

    byte *cursor = input.items + cursor_byte;
    byte *line_start = text.slice.items;
    byte *line_end = line_start + text.slice.count;
    if (cursor < line_start || cursor > line_end) return;
    if (cursor == line_end && cursor_byte < input.count) return;

    i32 cursor_x = text.x + brenda_text_measure_width(line_start, cursor - line_start);
    if (cursor_byte == input.count) {
        brenda_text_draw(cursor_x, text.y, " ", 1, effect);
        return;
    }

    isize next = cursor_byte + 1;
    while (next < input.count && ((u8)input.items[next] & 0xc0) == 0x80) next++;
    brenda_text_draw(cursor_x, text.y, cursor, next - cursor_byte, effect);
}

static inline void text_input_events_handle(Tui_TextInputState *input, Tui_TextInputResult *result) {
    for (isize i = 0; i < state.events.count; ++i) {
        Brenda_Event event = state.events.items[i];

        if (event.type == BRENDA_EVENT_UTF8
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
            continue;
        }

        if (event.type != BRENDA_EVENT_TERM_KEY) continue;
        switch (event.as.term_key) {
            case BRENDA_TERM_KEY_BACKSPACE: {
                if (input->cursor == 0) break;
                isize previous = utf8_previous_byte(input);
                memmove(input->items + previous, input->items + input->cursor, input->count - input->cursor);
                input->count -= input->cursor - previous;
                input->cursor = previous;
                result->changed = true;
                break;
            }
            case BRENDA_TERM_KEY_DELETE: {
                if (input->cursor == input->count) break;
                isize next = utf8_next_byte(input);
                memmove(input->items + input->cursor, input->items + next, input->count - next);
                input->count -= next - input->cursor;
                result->changed = true;
                break;
            }
            case BRENDA_TERM_KEY_LEFT: input->cursor = utf8_previous_byte(input); break;
            case BRENDA_TERM_KEY_RIGHT: input->cursor = utf8_next_byte(input); break;
            case BRENDA_TERM_KEY_HOME: input->cursor = 0; break;
            case BRENDA_TERM_KEY_END: input->cursor = input->count; break;
            case BRENDA_TERM_KEY_ENTER: result->submitted = true; break;
            default: break;
        }
    }
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
