#define PSH_BUILD_IMPL
#define LIBTUI_IMPL
#define PSH_NO_ECHO
    #include "../libtui.h"

#include <time.h>

void main_loop();
u32 clock_str(byte buffer[16]);

i32 main(i32 argc, byte *argv[]) {
    PSH_REBUILD_UNITY_AUTO(argc, argv);

    init_terminal();
    set_max_timeout_ms(1000);

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
    byte buffer[16] = {0};
    u32 len = clock_str(buffer);
    put_ascii_str(0, 0, buffer, len);
}

u32 clock_str(byte buffer[16]) {
    time_t now = time(NULL);
    struct tm *t = gmtime(&now);

    byte *p = buffer;

    if (t->tm_hour < 10) *p++ = '0';
    p += u32_to_ascii(p, t->tm_hour);
    *p++ = ':';

    if (t->tm_min < 10) *p++ = '0';
    p += u32_to_ascii(p, t->tm_min);
    *p++ = ':';
    
    if (t->tm_sec < 10) *p++ = '0';
    p += u32_to_ascii(p, t->tm_sec);

    return p - buffer;
}
