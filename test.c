#define PSH_BUILD_IMPL
#define LIBTUI_IMPL
#define PSH_NO_ECHO
    #include "libtui.h"

#define SEC(x)  x * 1000

Cmd cmd = {0};
u16 timer = 0;
Procs procs = {0};
Fd_Reader reader;
Sb buffer = {0};

void main_loop();
void parse_data();
void reset_reader();

i32 main(i32 argc, byte *argv[]) {
    PSH_REBUILD_UNITY_AUTO(argc, argv);

    init_terminal();
    set_max_timeout_ms(10);
    timer = SEC(10);
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
    timer += get_delta_time();
    if (timer >= SEC(5)) {
        timer = 0;

        Unix_Pipe pipe;
        assert(pipe_open(&pipe));
        reader.fd = pipe.read_fd;

        // closes write fd
        cmd.count = 3;
        cmd_run(&cmd, .fdout = pipe.write_fd, .async = &procs);
    }

    if (reader.fd != INVALID_FD)
        // closes read fd when done reading
        fd_read(&reader, .nonblocking = true);

    if (reader.ready) {
        // asserting this to one failed.
        //TODO: need to investigate!!!
        psh__proc_wait_async(procs.items[0]);
        procs.count = 0;

        parse_data();
        reset_reader();
    }

    put_ascii_str(0, 0, buffer.items, buffer.count);
}

void parse_data() {
    // {"data":["text"]}
    buffer.count = 0;
    sb_append_buf(&buffer, reader.store.items, reader.store.count);
}

void reset_reader() {
    reader.store.count = 0;
    reader.ready = false;
    reader.fd = INVALID_FD;
}