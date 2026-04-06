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

    Div scroll = div_new(0, 0);
    scroll.container_style.overflow = OVERFLOW_SCROLL_Y;
    // scroll.widget.style.w = 100;
    scroll.widget.style.border = 1;
    scroll.widget.style.padding = 1;
    // scroll.widget.style.align_self = ALIGN_CENTER;

    Div content = div_new(0,0);
    // content.container_style.direction = LAYOUT_ROW;
    //TODO: children overflow, align_end, overflow_scoll 
    //      allow to scroll down even though the overflow
    //      is above.
    content.container_style.spacing = 2;
    content.container_style.align_children = ALIGN_END;
    content.container_style.overflow = OVERFLOW_SCROLL_Y;

    // content.widget.style.padding = 5;
    content.widget.style.h = 30;
    // content.widget.style.w = 100;
    content.widget.style.border = 1;
    content.widget.style.align_self = ALIGN_CENTER;

    Button b1 = button_new(s8("item"));
    b1.widget.style.align_self = ALIGN_CENTER;
    Button b2 = button_new(s8("item"));
    Button b3 = button_new(s8("itemasdasdasd"));
    Button b4 = button_new(s8("item"));
    Button b5 = button_new(s8("item"));
    b5.widget.style.align_self = ALIGN_END;

    container_add(&content.widget, &b1.widget);
    container_add(&content.widget, &b2.widget);
    container_add(&content.widget, &b3.widget);
    container_add(&content.widget, &b4.widget);
    container_add(&content.widget, &b5.widget);

    container_add(&scroll.widget, &content.widget);

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
    Div layout = div_new(5, 1);

    s8 label1 = s8("What is Chandler Bing's job?");
    s8 label2 = s8("Transponster");
    s8 label3 = s8("That's not even a word..");

    Button b  = button_new(label1);
    Button b2 = button_new(label2);
    Button b3 = button_new(label3);

    ui_register_root(&layout.widget);
    container_add(&layout.widget, &b.widget);
    container_add(&layout.widget, &b2.widget);
    container_add(&layout.widget, &b3.widget);
}

void e2() {
    Div content = div_new(0,0);
    Button b1 = button_new(s8("item"));
    b1.widget.style.align_self = ALIGN_CENTER;
    Button b2 = button_new(s8("item"));
    Button b3 = button_new(s8("itemasdnakjsdkalsdjlnaskd"));
    Button b4 = button_new(s8("item"));
    Button b5 = button_new(s8("item"));
    b5.widget.style.align_self = ALIGN_END;

    // content.container_style.direction = LAYOUT_ROW;
    content.container_style.spacing = 10;
    content.container_style.align_children = ALIGN_START;
    content.container_style.overflow = OVERFLOW_SCROLL_Y;

    // content.widget.style.padding = 5;
    // content.widget.style.h = 30;
    // content.widget.style.w = 100;
    content.widget.style.border = 1;

    container_add(&content.widget, &b1.widget);
    container_add(&content.widget, &b2.widget);
    container_add(&content.widget, &b3.widget);
    container_add(&content.widget, &b4.widget);
    container_add(&content.widget, &b5.widget);

    Div scroll = div_new(0, 0);
    // scroll.widget.style.border = 1;
    scroll.widget.style.align_self = ALIGN_CENTER;
    container_add(&scroll.widget, &content.widget);

    ui_register_root(&scroll.widget);
}

void e3() {
    Div root = div_new(1,1);
    root.widget.style.border = 1;

    Button b = button_new(s8("Click"));
    TextInput input = text_input_new();

    container_add(&root.widget, &b.widget);
    container_add(&root.widget, &input.widget);

    ui_register_root(&root.widget);
}
