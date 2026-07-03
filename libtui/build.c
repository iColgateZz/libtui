#define PSH_CORE_IMPL
#include "psh_core/psh_core.h"

i32 main(i32 argc, byte *argv[]) {
    PSH_REBUILD_UNITY_AUTO(argc, argv);
    
    #define BUILD_DIR "build"
    Psh_Cmd cmd = {0};

    if (argc > 1) {
        byte *task = argv[1];
        if (strncmp(task, "clean", 5) == 0) {
            psh_cmd_append(&cmd, "rm", "-rf", BUILD_DIR);
            if (!psh_cmd_run(&cmd)) return 1;

            return 0;
        }

        psh_logger(PSH_ERROR, "unknown command: %s", task);
        return 1;
    }

    psh_cmd_append(&cmd, "mkdir", "-p", BUILD_DIR);
    if (!psh_cmd_run(&cmd)) return 1;

    byte *paths[] = {
        "main.c",
        // "layout.h",
        // "renderer.h"
    };

    psh_cmd_append(&cmd, PSH_CC_CMD(BUILD_DIR"/app", "main.c"));
    if (!psh_cmd_run(&cmd)) return 1;

    return 0;
}