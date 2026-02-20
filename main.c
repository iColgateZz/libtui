#define LIBTUI_IMPL
    #include "libtui.h"

#define PSH_BUILD_IMPL
    #include "psh_build/psh_build.h"

typedef struct {
    u32 x, y;
    u32 w, h;
    i32 state;
    u32 counter;
    u32 len;
} Widget;

static i32 y_offset = 0;

void draw(Widget *a) {
    u8 ch;
    if (a->state == 0) {
        ch = '-';
    } else {
        ch = '|';
    }

    for (usize i = 0; i < a->h; i++) {
        for (usize j = 0; j < a->w; j++) {
            put_char(a->x + j, a->y + i - y_offset, ch);
        }
    }
}

void update(Widget *a) {
    a->counter += get_delta_time();
    if (a->counter > 1000) {
        a->counter = 0;
        a->state = !a->state;
        a->x += 1;
    }
}

void on_winch(Widget *a) {
    a->x = get_terminal_width() / 2 - a->len / 2;
    a->y = get_terminal_height() / 2 - a->len / 2;
}

i32 main(i32 argc, byte *argv[]) {
    PSH_REBUILD_UNITY_AUTO(argc, argv);

    init_terminal();
    set_max_timeout_ms(1);

    u32 len = 6;
    Widget a = {
        .x = get_terminal_width() / 4 - len / 2,
        .y = get_terminal_height() / 2 - len / 2,
        .w = len,
        .h = len,
        .len = len,
        .state = 0,
        .counter = 0,
    };

    Widget b = {
        .x = get_terminal_width() / 4 * 3 - len / 2,
        .y = get_terminal_height() / 2 - len / 2,
        .w = len,
        .h = len,
        .len = len,
        .state = 0,
        .counter = 0,
    };

    // usize side = 88;
    // push_scope(0, 0, side, side);
    // push_scope(side - 1, 0, 1, 15);
    // push_scope(0, 0, 10, 10);

    while (!is_key_pressed('q')) {
        begin_frame();

        // application logic
        if (get_event_type() == EWinch) {
            on_winch(&a);
            on_winch(&b);
        }

        if (get_event_type() == EScrollDown) {
            y_offset++;
            // write_str("Here\r\n");
        } else if (get_event_type() == EScrollUp) {
            y_offset--;
            // write_str("Here2\r\n");
        }

        push_scope(0, 0, 88, 10);
        update(&a);
        draw(&a);
        pop_scope();

        update(&b);
        draw(&b);

        end_frame();
    }

    return 0;
}
