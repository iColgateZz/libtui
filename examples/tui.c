#define PSH_BUILD_IMPL
#define LIBTUI_IMPL
#define PSH_NO_ECHO
    #include "../libtui.h"

void main_loop();

Root root = {0};

i32 main(i32 argc, byte *argv[]) {
    PSH_REBUILD_UNITY_AUTO(argc, argv);

    init_terminal();
    set_max_timeout_ms(10);

    root = root_new();
    VBox layout = vbox_new(2, 2, 0, 0);

    s8 label1 = s8("Hello there!");
    s8 label2 = s8("Another button");

    Button b  = button_new(0,0,label1);
    Button b2 = button_new(0,0,label2);

    widget_add_child(&root.widget, &layout.widget);
    widget_add_child(&layout.widget, &b.widget);
    widget_add_child(&layout.widget, &b2.widget);

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
    widget_measure_tree(&root.widget);
    widget_layout_tree(&root.widget);
    widget_update_tree(&root.widget);
    widget_draw_tree(&root.widget);
}
