#define BUILD_DIR "build"
#define DEFAULT_EXECUTABLE BUILD_DIR "/app"
#define TUI_EXECUTABLE BUILD_DIR "/tui_app"

#define PSH_CC_MORE_FLAGS "-std=c99", "-Ilayla", "-Ibrenda", "-Itui", "-Ipsh_core", "-Wpedantic"
#define PSH_CORE_IMPL
#include "psh_core/psh_core.h"

i32 main(i32 argc, byte *argv[]) {
    PSH_REBUILD_UNITY_AUTO(argc, argv);

    Psh_Cmd cmd = {0};
    b32 run_tui = false;
    if (argc > 1) {
        byte *task = argv[1];
        if (strcmp(task, "clean") == 0) {
            psh_cmd_append(&cmd, "rm", "-rf", BUILD_DIR);
            if (!psh_cmd_run(&cmd)) return 1;

            return 0;
        }

        if (strcmp(task, "tui") == 0) {
            run_tui = true;
        } else {
            psh_logger(PSH_ERROR, "unknown command: %s", task);
            return 1;
        }
    }

    byte *default_source_files[] = {
        "main.c",
        "layla/src/layla.c",
        "brenda/src/brenda.c",
    };

    byte *tui_source_files[] = {
        "tui_main.c",
        "layla/src/layla.c",
        "brenda/src/brenda.c",
        "tui/src/tui.c",
    };

    byte **source_files = run_tui ? tui_source_files : default_source_files;
    usize source_count = run_tui ? psh_countof(tui_source_files) : psh_countof(default_source_files);
    byte *executable = run_tui ? TUI_EXECUTABLE : DEFAULT_EXECUTABLE;
    Psh_C_Build build = {
        .build_dir = BUILD_DIR,
        .output = executable,
        .sources = source_files,
        .source_count = source_count,
        .max_procs = 10,
    };
    if (!psh_c_build_run(&build)) return 1;

    psh_cmd_append(&cmd, executable);
    psh_shift(argv, argc);
    if (run_tui) psh_shift(argv, argc);
    psh_list_append_many(&cmd, argv, argc);
    if (!psh_cmd_run(&cmd)) return 1;

    return 0;
}
