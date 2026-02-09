#define LIBTUI_IMPL
    #include "libtui.h"

#define PSH_BUILD_IMPL
    #include "psh_build/psh_build.h"

#include <ctype.h>

i32 main(i32 argc, byte *argv[]) {
    PSH_REBUILD_UNITY_AUTO(argc, argv);

    init_term();

    while (1) {
        char c = 0;
        read(STDIN_FILENO, &c, 1);

        if (iscntrl(c)) {
            printf("%d\r\n", c);
        } else {
            printf("%d ('%c')\r\n", c, c);
        }

        if (c == 'q') break;
    }

    return 0;
}