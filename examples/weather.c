#define PSH_BUILD_IMPL
#define LIBTUI_IMPL
#define PSH_NO_ECHO
    #include "../libtui.h"


//TODO: mv s8 to psh_core

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

    s8 msg = s8("Well done!");
    put_ascii_str(0, 1, msg.s, msg.len);
}

void print_dimensions() {
    byte buffer[64];
    byte *p = buffer;

    //TODO: simplify formatting
    s8 width_str = s8("Width: ");
    memcpy(p, width_str.s, width_str.len);
    p += width_str.len;

    p += u32_to_ascii(p, get_terminal_width());

    s8 height_str = s8("; height: ");
    memcpy(p, height_str.s, height_str.len);
    p += height_str.len;

    p += u32_to_ascii(p, get_terminal_height());

    put_ascii_str(0, 0, buffer, p - buffer);
}

b32 check_min_dimensions() {
    u32 w = get_terminal_width();
    u32 h = get_terminal_height();

    if (w >= 60 && h >= 20) return true;

    s8 msg = s8("Something went wrong!");
    put_ascii_str(0, 1, msg.s, msg.len);
    return false;
}
