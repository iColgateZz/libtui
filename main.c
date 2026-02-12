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
        write_str("-");
    } else {
        write_str("|");
    }
}

i32 main(i32 argc, byte *argv[]) {
    PSH_REBUILD_UNITY_AUTO(argc, argv);

    init_term();
    set_target_fps(60);

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

    add_widget(&a);

    while (!key_pressed('q')) {
        save_timestamp();

        // capture events
        poll_input();

        a.counter += Terminal.dt;
        if (a.counter > 1000) {
            a.counter = 0;
            a.state = !a.state;
        }
        
        // draw ui
        draw_widgets();

        calculate_dt();
        // printf("dt: %llu\r\n", Terminal.dt);
    }

    return 0;
}
