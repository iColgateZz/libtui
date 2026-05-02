#define PSH_BUILD_IMPL
#define LIBTUI_RENDERER_IMPL
#define PSH_NO_ECHO
    #include "../libtui/renderer.h"

CodePoint last_cp;

void main_loop();

i32 main(i32 argc, byte *argv[]) {
    PSH_REBUILD_UNITY_AUTO(argc, argv);

    init_terminal();
    set_max_timeout_ms(10);

    last_cp = cp("A");

    while (!is_codepoint(ctrl('x'))) {
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

    // put_cp(0, 0, last_cp);
    // put_cp(1, 0, cp("p"));

    put_cp(0, 0, cp("😂"));
    put_cp(1, 0, last_cp);

    Scratch scratch = scratch_get();

    Stream s = arena_stream_start(scratch.arena, 256);
    stream_fmt(&s, "Width: %u; ", last_cp.display_width);
    stream_fmt(&s, "Raw len: %u", last_cp.raw_len);
    s8 res = stream_end(s);

    put_str(0, 1, res.s, res.len);

    scratch_end(scratch);
}
