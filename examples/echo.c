#define PSH_BUILD_IMPL
#define LIBTUI_IMPL
#define PSH_NO_ECHO
    #include "../libtui.h"

CodePoint last_cp;

void main_loop();

i32 main(i32 argc, byte *argv[]) {
    PSH_REBUILD_UNITY_AUTO(argc, argv);

    init_terminal();
    set_max_timeout_ms(10);

    last_cp = cp("A");

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
    if (is_event(ECodePoint)) {
        last_cp = get_codepoint();
    }

    // put_codepoint(0, 0, last_cp);
    // put_codepoint(1, 0, cp("p"));

    put_codepoint(0, 0, cp("😂"));
    put_codepoint(1, 0, last_cp);

    //TODO: Make this more ergonomic from a user perspective
    Scratch s = scratch_get();
    byte *buffer = arena_push(s.arena, byte, 256);
    byte *p = buffer;

    p = fmt(p, buffer + 256, "Width: %u; ", last_cp.display_width);
    p = fmt(p, buffer + 256, "Raw len: %u", last_cp.raw_len);
    put_str(0, 1, buffer, p - buffer);

    scratch_end(s);
}
