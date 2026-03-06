#define PSH_BUILD_IMPL
#define LIBTUI_IMPL
#define PSH_NO_ECHO
    #include "../libtui.h"


//TODO: mv s8 to psh_core
#include <stddef.h>

typedef struct {
    byte *s;
    usize len;
} s8;

#define countof(x)              (usize)(sizeof(x) / sizeof(*(x)))
#define lenof(s)                countof(s) - 1

#define s8(...)                       s8_(__VA_ARGS__, s8_from_heap, s8_read_only)(__VA_ARGS__)
#define s8_(a, b, c, ...)             c
#define s8_read_only(str)             (s8){.s = str, .len = lenof(str)}
#define s8_from_heap(str, str_len)    (s8){.s = str, .len = str_len}

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
