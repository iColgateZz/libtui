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
    ui_init();

    // e1();
    e2();
    // e3();

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
    Div *layout = div_new(5, 1);
    layout->widget.style.border = 1;

    s8 label1 = s8("What is Chandler Bing's job?");
    s8 label2 = s8("Transponster");
    s8 label3 = s8("That's not even a word..");

    Button *b  = button_new(label1);
    b->widget.style.border = 1;
    Button *b2 = button_new(label2);
    b2->widget.style.border = 1;
    Button *b3 = button_new(label3);
    b3->widget.style.border = 1;

    ui_register_root(&layout->widget);
    container_add(&layout->widget, &b->widget);
    container_add(&layout->widget, &b2->widget);
    container_add(&layout->widget, &b3->widget);
}

void e2() {
   Div *scroll = div_new(0, 0);
    scroll->container_style.overflow = OVERFLOW_SCROLL_Y;
    // scroll.widget.style.w = 100;
    scroll->widget.style.border = 1;
    scroll->widget.style.padding = 1;
    scroll->widget.style.align_self = ALIGN_CENTER;
    // scroll->widget.style.margin = 3;

    Div *content = div_new(0,0);
    // content->container_style.direction = LAYOUT_ROW;
    content->container_style.spacing = 10;
    content->container_style.align_children = ALIGN_START;
    content->container_style.overflow = OVERFLOW_SCROLL_Y;

    // content->widget.style.padding = 5;
    // content->widget.style.margin = 2;
    // content->widget.style.h = 30;
    // content->widget.style.w = 100;
    content->widget.style.border = 1;
    content->widget.style.align_self = ALIGN_CENTER;

    Button *b1 = button_new(s8("item"));
    b1->widget.style.align_self = ALIGN_CENTER;
    b1->widget.style.border = 1;
    Button *b2 = button_new(s8("item"));
    b2->widget.style.border = 1;
    Button *b3 = button_new(s8("itemasdasdasd"));
    b3->widget.style.border = 1;
    Button *b4 = button_new(s8("item"));
    b4->widget.style.border = 1;
    Button *b5 = button_new(s8("item"));
    b5->widget.style.align_self = ALIGN_END;
    b5->widget.style.border = 1;

    container_add(&content->widget, &b1->widget);
    container_add(&content->widget, &b2->widget);
    container_add(&content->widget, &b3->widget);
    container_add(&content->widget, &b4->widget);
    container_add(&content->widget, &b5->widget);

    container_add(&scroll->widget, &content->widget);

    ui_register_root(&scroll->widget);
}

void e3() {
    Div *root = div_new(1,0);
    root->widget.style.border = 1;

    Button *b = button_new(s8("Click"));
    b->widget.style.border = 1;

    TextInput *input = text_input_new();
    input->widget.style.border = 1;
    input->widget.style.padding = 1;

    container_add(&root->widget, &b->widget);
    container_add(&root->widget, &input->widget);

    ui_register_root(&root->widget);
}
