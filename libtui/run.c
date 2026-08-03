#define BUILD_DIR "build"
#define DEFAULT_EXECUTABLE BUILD_DIR "/app"
#define TUI_EXECUTABLE BUILD_DIR "/tui_app"
#define PATH_CAPACITY 256

#define PSH_CC_MORE_FLAGS "-std=c99", "-Ilayla", "-Ibrenda", "-Itui", "-Ipsh_core", "-Wpedantic"
#define PSH_CORE_IMPL
#include "psh_core/psh_core.h"

//TODO: something should definitely go to psh_core.
static psh_ternary object_needs_rebuild(byte *object, byte *source) {
    struct stat statbuf = {0};
    if (stat(object, &statbuf) < 0) {
        if (errno == ENOENT) return true;

        psh_logger(PSH_ERROR, "could not get info about object %s: %s", object, strerror(errno));
        return psh_err;
    }

    Psh_Unix_Pipe pipe = {0};
    if (!psh_pipe_open(&pipe)) return psh_err;

    Psh_Cmd cmd = {0};
    psh_cmd_append(&cmd, PSH_CC, PSH_CC_MORE_FLAGS, "-MM", source);
    if (!psh_cmd_run(&cmd, .fdout = pipe.write_fd)) {
        psh_list_free(cmd);
        return psh_err;
    }
    psh_list_free(cmd);

    Psh_Fd_Reader reader = {.fd = pipe.read_fd};
    if (!psh_fd_read(&reader)) {
        psh_list_free(reader.store);
        return psh_err;
    }

    Sources dependencies = psh__tokenize_deps(reader.store.count, reader.store.items);
    psh_ternary result = psh__needs_rebuild(object, dependencies.items, dependencies.count);

    psh_list_free(dependencies);
    psh_list_free(reader.store);
    return result;
}

static b32 make_object_path(byte *source, byte *result, usize result_size) {
    usize source_len = strlen(source);
    if (source_len < 3 || strcmp(source + source_len - 2, ".c") != 0) {
        psh_logger(PSH_ERROR, "source file must end in .c: %s", source);
        return false;
    }

    i32 written = snprintf(result, result_size, BUILD_DIR"/%.*s.o", (i32)(source_len - 2), source);
    if (written < 0 || (usize)written >= result_size) {
        psh_logger(PSH_ERROR, "object path is too long for source: %s", source);
        return false;
    }

    return true;
}

static b32 make_object_directory(byte *object) {
    byte directory[PATH_CAPACITY] = {0};
    usize object_len = strlen(object);
    if (object_len >= sizeof(directory)) return false;

    memcpy(directory, object, object_len + 1);
    byte *last_slash = strrchr(directory, '/');
    PSH_ASSERT(last_slash != NULL);
    *last_slash = 0;

    Psh_Cmd cmd = {0};
    psh_cmd_append(&cmd, "mkdir", "-p", directory);
    b32 result = psh_cmd_run(&cmd);
    psh_list_free(cmd);
    return result;
}

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

    psh_cmd_append(&cmd, "mkdir", "-p", BUILD_DIR);
    if (!psh_cmd_run(&cmd)) return 1;

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
    byte object_paths[source_count][PATH_CAPACITY];
    byte *object_files[source_count];
    b32 object_rebuilt = false;

    Psh_Procs procs = {0};
    for (usize i = 0; i < source_count; ++i) {
        byte *source = source_files[i];
        byte *object = object_paths[i];
        object_files[i] = object;

        if (!make_object_path(source, object, PATH_CAPACITY)) return 1;

        psh_ternary needs_rebuild = object_needs_rebuild(object, source);
        if (needs_rebuild == psh_err) return 1;
        if (!needs_rebuild) continue;

        if (!make_object_directory(object)) return 1;

        psh_cmd_append(&cmd, PSH_CC, PSH_CC_FLAGS, PSH_CC_MORE_FLAGS, "-c", source, "-o", object);
        if (!psh_cmd_run(&cmd, .async = &procs, .max_procs = 10)) return 1;
        object_rebuilt = true;
    }

    if (!psh_procs_block(&procs)) return 1;

    psh_ternary link_executable = object_rebuilt ? true : psh__needs_rebuild(executable, object_files, source_count);
    if (link_executable == psh_err) return 1;

    if (link_executable) {
        psh_cmd_append(&cmd, PSH_CC, PSH_CC_FLAGS, PSH_CC_MORE_FLAGS, "-o", executable);
        psh_list_append_many(&cmd, object_files, source_count);
        if (!psh_cmd_run(&cmd)) return 1;
    }

    psh_cmd_append(&cmd, executable);
    psh_shift(argv, argc);
    if (run_tui) psh_shift(argv, argc);
    psh_list_append_many(&cmd, argv, argc);
    if (!psh_cmd_run(&cmd)) return 1;

    return 0;
}
