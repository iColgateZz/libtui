#define LIBTUI_IMPL
    #include "libtui.h"

#define PSH_BUILD_IMPL
    #include "psh_build/psh_build.h"

#include <ctype.h>

void print_char(Key k) {
    if (k < 0) {
        write_strf("Escaped sequence: %d\r\n", k);
    } else if (k < 32) {
        write_strf("Non-printable char: %d\r\n", k);
    } else {
        write_strf("Regular char: %c, %d\r\n", k, k);
    }
}

i32 main(i32 argc, byte *argv[]) {
    PSH_REBUILD_UNITY_AUTO(argc, argv);

    init_term();
    set_target_fps(1);

    while (1) {
        // capture events
        poll_input();

        // update state & other use logic
        Key k = get_key();
        print_char(k);

        printf("%d, %d\r\n", Terminal.width, Terminal.height);

        if (k == 'q') break;

        // draw ui
    }

    return 0;
}
