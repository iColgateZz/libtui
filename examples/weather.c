#define PSH_BUILD_IMPL
#define LIBTUI_IMPL
#define PSH_NO_ECHO
    #include "../libtui.h"

void main_loop();
void print_dimensions();
b32 check_min_dimensions();

i32 main(i32 argc, byte *argv[]) {
    PSH_REBUILD_UNITY_AUTO(argc, argv);

    init_terminal();
    set_max_timeout_ms(20);

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
    print_dimensions();
    if (!check_min_dimensions()) return;

    s8 msg = s8("аоцуоадц лоаоцлуÄääääääää");
    put_str(0, 1, msg.s, msg.len);
    // put_str(0, 1, "\xF8", 1);
}

void print_dimensions() {
    byte buffer[64];
    Stream s = stream_start(buffer, 64);

    stream_fmt(&s, "Width: %u; Heigth: %u", get_terminal_width(), get_terminal_height());
    s8 res = stream_end(s);

    put_ascii_str(0, 0, res.s, res.len);
}

b32 check_min_dimensions() {
    u32 w = get_terminal_width();
    u32 h = get_terminal_height();

    if (w >= 60 && h >= 20) return true;

    s8 msg = s8("Something went wrong!");
    put_ascii_str(0, 1, msg.s, msg.len);
    return false;
}
