#define LIBTUI_IMPL
    #include "libtui.h"

#define PSH_BUILD_IMPL
    #include "psh_build/psh_build.h"

#include <ctype.h>

void print_char(Key k) {
    if (k < 0) {
        write_strf("Escaped sequence: %d\r\n", k);
    } else if (k < 32) {
        write_strf("Non-printable char: %d\r\n", k);
    } else {
        write_strf("Regular char: %c, %d\r\n", k, k);
    }
}

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

    while (1) {
        save_timestamp();

        // capture events
        poll_input();

        a.counter += Terminal.dt;
        if (a.counter > 1000) {
            a.counter = 0;
            a.state = !a.state;
        }

        // update state & other user logic
        // Key k = get_key();
        // print_char(k);
        
        if (key_pressed('q')) break;
        
        // draw ui
        draw_widgets();

        calculate_dt();
        // printf("dt: %llu\r\n", Terminal.dt);
    }

    return 0;
}
