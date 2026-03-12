#define PSH_BUILD_IMPL
#define LIBTUI_IMPL
#define PSH_NO_ECHO
    #include "../libtui.h"

void main_loop();
void print_dimensions();
b32 check_min_dimensions();
void print_house();

typedef struct {
    usize counter;
    u32 x, y;
} Bird;

Bird bird = {
    .y = 5,
};

void print_bird();

// 20 x 8
static const byte house[] = {
"      `'::.\n"
"  _________H ,%%&%,\n"
" /\\     _   \\%&&%%&%\n"
"/  \\___/^\\___\\%&%%&&\n"
"|  | []   [] |%\\Y&%'\n"
"|  |   .-.   | ||\n"
"@._|@@_|||_@@|~||\n"
"   `\"\"\") )\"\"\"`\n"
};

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

    s8 msg = s8("Normal weather");
    put_str(0, 1, msg.s, msg.len);
    // put_str(0, 1, "\xF8", 1);

    print_house();
    print_bird();
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

    s8 msg = s8("Terminal size is too small");
    put_ascii_str(0, 1, msg.s, msg.len);
    return false;
}

void print_house() {
    usize x = (get_terminal_width() - 20) / 2;
    usize y = (get_terminal_height() - 8) / 2;

    usize ground_level = y + 6;
    for (usize i = 0; i < get_terminal_width(); i++) {
        put_ascii_char(i, ground_level, '~');
    }

    byte *p = (byte *)house;
    byte *line = p;
    while (*p) {
        if (*p == '\n') {
            put_str(x, y++, line, p - line);
            line = p + 1;
        }
        p++;
    }
}

void print_bird() {
    bird.counter += get_delta_time();

    if (bird.counter > 100) {
        bird.counter -= 100;
        bird.x++;

        if (bird.x >= get_terminal_width()) {
            bird.x = 0;
        }
    }

    put_ascii_char(bird.x, bird.y, '>');
}
