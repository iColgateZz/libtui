#define PSH_BUILD_IMPL
#define LIBTUI_IMPL
#define PSH_NO_ECHO
    #include "../libtui.h"

void main_loop();

Button b;

i32 main(i32 argc, byte *argv[]) {
    PSH_REBUILD_UNITY_AUTO(argc, argv);

    init_terminal();
    set_max_timeout_ms(10);

    s8 label = s8("Hello there!");
    b = button_new(5, 5, 15, 3, label);

    while (!is_codepoint(cp("q"))) {
        begin_frame();
        {
            main_loop();
        }
        end_frame();
    }

    return 0;
}

void main_loop() {
    widget_draw(&b.widget);
}
