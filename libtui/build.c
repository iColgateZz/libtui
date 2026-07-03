#define PSH_CORE_IMPL
#include "psh_core/psh_core.h"

#define BUILD_DIR "build"
#define EXECUTABLE BUILD_DIR "/app"
#define PATH_CAPACITY 256

//TODO: something should definitely go to psh_core.
static psh_ternary object_needs_rebuild(byte *object, byte *source) {
    struct stat statbuf = {0};
    if (stat(object, &statbuf) < 0) {
        if (errno == ENOENT) return true;

        psh_logger(PSH_ERROR, "could not get info about object %s: %s", object, strerror(errno));
        return err;
    }

    Psh_Unix_Pipe pipe = {0};
    if (!psh_pipe_open(&pipe)) return err;

    Psh_Cmd cmd = {0};
    psh_cmd_append(&cmd, PSH_CC, "-MM", source);
    if (!psh_cmd_run(&cmd, .fdout = pipe.write_fd)) {
        psh_list_free(cmd);
        return err;
    }
    psh_list_free(cmd);

    Psh_Fd_Reader reader = {.fd = pipe.read_fd};
    if (!psh_fd_read(&reader)) {
        psh_list_free(reader.store);
        return err;
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
    if (argc > 1) {
        byte *task = argv[1];
        if (strcmp(task, "clean") == 0) {
            psh_cmd_append(&cmd, "rm", "-rf", BUILD_DIR);
            if (!psh_cmd_run(&cmd)) return 1;

            return 0;
        }

        psh_logger(PSH_ERROR, "unknown command: %s", task);
        return 1;
    }

    psh_cmd_append(&cmd, "mkdir", "-p", BUILD_DIR);
    if (!psh_cmd_run(&cmd)) return 1;

    byte *source_files[] = {
        "main.c",
    };

    usize source_count = psh_countof(source_files);
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
        if (needs_rebuild == err) return 1;
        if (!needs_rebuild) continue;

        if (!make_object_directory(object)) return 1;

        psh_cmd_append(
            &cmd,
            PSH_CC, PSH_CC_FLAGS, PSH_CC_MORE_FLAGS,
            "-c", source, "-o", object
        );
        if (!psh_cmd_run(&cmd, .async = &procs, .max_procs = 10)) return 1;
        object_rebuilt = true;
    }

    if (!psh_procs_block(&procs)) return 1;

    b32 link_executable = object_rebuilt;
    if (!link_executable) {
        struct stat statbuf = {0};
        if (stat(EXECUTABLE, &statbuf) < 0) {
            if (errno != ENOENT) {
                psh_logger(PSH_ERROR, "could not get info about executable %s: %s", EXECUTABLE, strerror(errno));
                return 1;
            }
            link_executable = true;
        }
    }

    if (link_executable) {
        psh_cmd_append(&cmd, PSH_CC, PSH_CC_FLAGS, PSH_CC_MORE_FLAGS, "-o", EXECUTABLE);
        psh_list_append_many(&cmd, object_files, source_count);
        if (!psh_cmd_run(&cmd)) return 1;
    }

    return 0;
}
