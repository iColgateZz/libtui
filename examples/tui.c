#define PSH_BUILD_IMPL
#define LIBTUI_IMPL
#define PSH_NO_ECHO
    #include "../libtui.h"

void e1();
void e2();
void e3();
void e4();

i32 main(i32 argc, byte *argv[]) {
    PSH_REBUILD_UNITY_AUTO(argc, argv);

    init_terminal();
    set_max_timeout_ms(10);
    ui_init();

    // e1();
    // e2();
    // e3();
    e4();

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
    Div *layout = div_new();
    style(layout, 
        border_width(1),
    );

    s8 label1 = s8("What is Chandler Bing's job?");
    s8 label2 = s8("Transponster");
    s8 label3 = s8("That's not even a word..");

    Button *b  = button_new(label1);
    style(b, 
        border_width(1),
    );
    Button *b2 = button_new(label2);
    style(b2, 
        border_width(1),
    );
    Button *b3 = button_new(label3);
    style(b3, 
        border_width(1),
    );

    ui_register_root(&layout->widget);
    div_add(layout, &b->widget);
    div_add(layout, &b2->widget);
    div_add(layout, &b3->widget);
}

void e2() {
    Div *scroll = div_new();
    style(scroll, 
        // width(100),
        border_width(1),
        padding(1),
        align_self(ALIGN_CENTER),
        // margin(3),
        border_bg(127, 10, 15),
        // border_bold(true),
        bg(127, 10, 15),
    );
    div_style(scroll,
        overflow(OVERFLOW_SCROLL_Y),
    );

    Div *content = div_new();
    style(content,
        // padding(5),
        // margin(2),
        // height(30),
        // width(100),
        border_width(1),
        align_self(ALIGN_CENTER),
    );
    div_style(content,
        // direction(LAYOUT_ROW),
        spacing(10),
        align_children(ALIGN_START),
        overflow(OVERFLOW_SCROLL_Y),
    );
    
    StyleArgs args = style_new(
        border_width(1),
    );

    Button *b1 = button_new(s8("item"));
    style_args(b1, args,
        align_self(ALIGN_CENTER),
    );

    Button *b2 = button_new(s8("item"));
    style_args(b2, args);

    Button *b3 = button_new(s8("itemasdasdasd"));
    style_args(b3, args,
        border_fg(127, 10, 15),
        // text_bold(true),
    );

    Button *b4 = button_new(s8("item"));
    style_args(b4, args);

    Button *b5 = button_new(s8("item"));
    style_args(b5, args,
        align_self(ALIGN_END),
    );

    div_add(content, &b1->widget);
    div_add(content, &b2->widget);
    div_add(content, &b3->widget);
    div_add(content, &b4->widget);
    div_add(content, &b5->widget);

    div_add(scroll, &content->widget);

    ui_register_root(&scroll->widget);
}

void e3() {
    Div *root = div_new();
    style(root, border_width(1));

    Button *b = button_new(s8("Click"));
    style(b, border_width(1));

    TextInput *input = text_input_new();
    style(input, border_width(1), padding(1), width(40));
    text_style(&input->input, text_align(ALIGN_CENTER));

    div_add(root, &b->widget);
    div_add(root, &input->widget);

    ui_register_root(&root->widget);
}

void e4() {
    Div *parent = div_new();
    style(parent, 
        // width(100),
        // border_width(3),
        // padding(1),
        // align_self(ALIGN_CENTER),
        // margin(3),
        // border_bg(127, 10, 15),
        // border_bold(true),
        bg(127, 10, 15),
        fill_x(),
        fill_y(),
    );
    div_style(parent,
        overflow(OVERFLOW_CLIP),
    );

    Div *child1 = div_new();
    style(child1,
        // padding(5),
        // margin(2),
        // height(10),
        // width(100),
        // border_width(1),
        // align_self(ALIGN_CENTER),
        bg(133, 132, 131),
        fill_x(),
        fill_y(),
    );

    Div *child2 = div_new();
    style(child2,
        // padding(5),
        // margin(2),
        // height(10),
        // width(100),
        // border_width(1),
        // align_self(ALIGN_CENTER),
        bg(133, 132, 255),
        fill_x(),
        fill_y(),
    );
    
    div_add(parent, &child1->widget);
    div_add(parent, &child2->widget);
    ui_register_root(&parent->widget);
}
