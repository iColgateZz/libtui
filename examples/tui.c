#define PSH_BUILD_IMPL
#define LIBTUI_IMPL
#define PSH_NO_ECHO
    #include "../libtui.h"

void e1();
void e2();
void e3();

i32 main(i32 argc, byte *argv[]) {
    PSH_REBUILD_UNITY_AUTO(argc, argv);

    init_terminal();
    set_max_timeout_ms(10);

    Div content = div_new(1,1, false);
    Button b1 = button_new(s8("item"));
    Button b2 = button_new(s8("item"));
    Button b3 = button_new(s8("item"));
    Button b4 = button_new(s8("item"));
    Button b5 = button_new(s8("item"));

    content.container_style.direction = LAYOUT_ROW;
    content.container_style.spacing = 5;

    // content.widget.style.padding = 5;
    content.widget.style.h = 40;

    div_add(&content, &b1.widget);
    div_add(&content, &b2.widget);
    div_add(&content, &b3.widget);
    div_add(&content, &b4.widget);
    div_add(&content, &b5.widget);

    Div scroll = div_new(0, 0, true);
    div_add(&scroll, &content.widget);

    ui_register_root(&scroll.widget);

    while (!is_codepoint(cp("q"))) {
        begin_frame();
        {
            ui_run();
        }
        end_frame();
    }

    return 0;
}

void e1() {
    Div layout = div_new(5, 1, false);

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
}

void e2() {
    Div content = div_new(1,1, false);
    Button b1 = button_new(s8("item"));
    Button b2 = button_new(s8("item"));
    Button b3 = button_new(s8("item"));
    Button b4 = button_new(s8("item"));
    Button b5 = button_new(s8("item"));

    div_add(&content, &b1.widget);
    div_add(&content, &b2.widget);
    div_add(&content, &b3.widget);
    div_add(&content, &b4.widget);
    div_add(&content, &b5.widget);

    Div scroll = div_new(0, 0, true);
    div_add(&scroll, &content.widget);

    ui_register_root(&scroll.widget);
}

void e3() {
    Div root = div_new(1,1, false);

    Button b = button_new(s8("Click"));
    TextInput input = text_input_new();

    div_add(&root, &b.widget);
    div_add(&root, &input.widget);

    ui_register_root(&root.widget);
}
