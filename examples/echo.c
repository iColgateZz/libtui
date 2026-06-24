#define LIBTUI_RENDERER_IMPL
    #include "../libtui/renderer.h"

#define PSH_NO_ECHO
#define PSH_CORE_IMPL
    #include "../libtui/psh_core/psh_core.h"

CodePoint last_cp;

void main_loop();

i32 main(i32 argc, byte *argv[]) {
    REBUILD_UNITY_AUTO(argc, argv);

    init_terminal();
    set_fps(60);

    last_cp = cp("A");

    b32 quit = false;
    while (!quit) {
        begin_frame();
        {
            Slice(Event) events = get_events();
            for (isize i = 0; i < events.count; ++i) {
                Event event = events.items[i];
                if (event_is_codepoint(event, ctrl('x'))) quit = true;
                if (event_is(event, ECodePoint)) last_cp = event.codepoint;
            }
            main_loop();
        }
        end_frame();
    }

    return 0;
}

void main_loop() {
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
