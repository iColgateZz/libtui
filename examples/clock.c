#define LIBTUI_RENDERER_IMPL
    #include "../libtui/renderer.h"

#define PSH_NO_ECHO
#define PSH_CORE_IMPL
    #include "../libtui/psh_core/psh_core.h"

#include <time.h>
#define buf_size 16

void main_loop();
u32 clock_str(byte buffer[buf_size]);

i32 main(i32 argc, byte *argv[]) {
    REBUILD_UNITY_AUTO(argc, argv);

    init_terminal();
    set_fps(60);

    b32 quit = false;
    while (!quit) {
        begin_frame();
        {
            Slice(Event) events = get_events();
            for (isize i = 0; i < events.count; ++i) {
                if (event_is_codepoint(events.items[i], cp("q"))) quit = true;
            }
            main_loop();
        }
        end_frame();
    }

    return 0;
}

void main_loop() {
    byte buffer[buf_size] = {0};
    u32 len = clock_str(buffer);
    put_str(0, 0, buffer, len);
}

u32 clock_str(byte buffer[buf_size]) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    byte *p = buffer;
    byte *end = buffer + buf_size;

    if (t->tm_hour < 10) *p++ = '0';
    p = fmt(p, end, "%u:", t->tm_hour);

    if (t->tm_min < 10) *p++ = '0';
    p = fmt(p, end, "%u:", t->tm_min);
    
    if (t->tm_sec < 10) *p++ = '0';
    p = fmt(p, end, "%u", t->tm_sec);

    return p - buffer;
}
