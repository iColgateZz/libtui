#define LIBTUI_IMPL
    #include "libtui.h"

#define PSH_BUILD_IMPL
    #include "psh_build/psh_build.h"

typedef struct {
    Widget w;
    i32 state;
    u32 counter;
} AnimationWidget;

void draw(Widget *w) {
    AnimationWidget *a = (AnimationWidget *)w;
    if (a->state == 0) {
        byte *text = "Some text!";
        put_str(0, 0, text, strlen(text));
    } else {
        for (usize i = 0; i < 10; i++)
            put_char(i, 0, '|');
    }
}

i32 main(i32 argc, byte *argv[]) {
    PSH_REBUILD_UNITY_AUTO(argc, argv);

    init_terminal();
    set_max_timeout_ms(16);

    Widget w = {
        .w = 10,
        .h = 5,
        .draw = draw,
    };

    AnimationWidget a = {
        .w = w,
        .state = 0,
        .counter = 0,
    };

    while (!is_key_pressed('q')) {
        begin_frame();

        // application logic
        a.counter += get_delta_time();
        if (a.counter > 1000) {
            a.counter = 0;
            a.state = !a.state;
        }

        a.w.draw(&a.w);

        end_frame();
    }

    return 0;
}
