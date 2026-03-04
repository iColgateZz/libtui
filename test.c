#define PSH_BUILD_IMPL
#define LIBTUI_IMPL
#define PSH_NO_ECHO
    #include "libtui.h"

#define SEC(x)  x * 1000

typedef struct {
    u16 timer;
    Unix_Pipe pipe;
} Asker;

Cmd cmd = {0};
Asker asker = {0};
Procs procs = {0};
Fd_Reader reader;
Sb buffer = {0};

void main_loop();

i32 main(i32 argc, byte *argv[]) {
    PSH_REBUILD_UNITY_AUTO(argc, argv);

    init_terminal();
    set_max_timeout_ms(100);
    asker.timer = SEC(10);
    cmd_append(&cmd, "curl", "-s", "https://meowfacts.herokuapp.com/");

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
    asker.timer += get_delta_time();
    if (asker.timer >= SEC(5)) {
        asker.timer = 0;

        assert(pipe_open(&asker.pipe));

        // closes write fd
        cmd.count = 3;
        cmd_run(
            &cmd, 
            .fdout = asker.pipe.write_fd,
            .async = &procs
        );

        reader.fd = asker.pipe.read_fd;
    }

    // closes read fd when done reading
    if (reader.fd != INVALID_FD)
        fd_read(&reader, .nonblocking = true);

    if (reader.ready) {
        // asserting this to one failed.
        //TODO: need to investigate!!!
        psh__proc_wait_async(procs.items[0]);
        procs.count = 0;

        reader.ready = false;
        reader.fd = INVALID_FD;
        buffer.count = 0;
        sb_append_buf(&buffer, reader.store.items, reader.store.count);
        reader.store.count = 0;
    }

    put_ascii_str(0, 0, buffer.items, buffer.count);
}