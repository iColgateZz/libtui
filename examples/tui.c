#define PSH_BUILD_IMPL
#define LIBTUI_IMPL
#define PSH_NO_ECHO
    #include "../libtui.h"

i32 main(i32 argc, byte *argv[]) {
    PSH_REBUILD_UNITY_AUTO(argc, argv);

    init_terminal();
    set_max_timeout_ms(10);

    Div layout = div_new(5, 5);

    s8 label1 = s8("What is Chandler Bing's job?");
    s8 label2 = s8("Transponster");
    s8 label3 = s8("That's not even a word..");

    Button b  = button_new(label1);
    Button b2 = button_new(label2);
    Button b3 = button_new(label3);

    ui_register_root(&layout.widget);
    div_add(&layout, &b.widget);
    div_add(&layout, &b2.widget);
    div_add(&layout, &b3.widget);

    while (!is_codepoint(cp("q"))) {
        begin_frame();
        {
            ui_run();
        }
        end_frame();
    }

    return 0;
}
