#define PSH_BUILD_IMPL
#define LIBTUI_IMPL
#define PSH_NO_ECHO
    #include "../libtui.h"

#define SEC(x)  x * 1000
#define SEC_PER_QUOTE 10

Cmd cmd = {0};
u16 timer = 0;
Procs procs = {0};
Fd_Reader reader;
Sb buffer = {0};
i32 result = 0;

void main_loop();
void parse_data();
void reset_reader();
void draw();

i32 main(i32 argc, byte *argv[]) {
    PSH_REBUILD_UNITY_AUTO(argc, argv);

    init_terminal();
    set_max_timeout_ms(10);
    timer = SEC(SEC_PER_QUOTE);
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
    if (timer >= SEC(SEC_PER_QUOTE)) {
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

    if (reader.ready && psh__proc_wait_async(procs.items[0]) > 0) {
        procs.count = 0;

        parse_data();
        reset_reader();
    }

    draw();
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

void draw() {
    u32 printed_lines = 0;
    while (printed_lines * get_terminal_width() < buffer.count) {
        u32 print_len = MIN(get_terminal_width(), buffer.count - printed_lines * get_terminal_width());
        put_str(
            0,
            printed_lines,
            buffer.items + printed_lines * get_terminal_width(),
            print_len
        );

        printed_lines++;
    }

    put_codepoint(0, 7, cp_from_byte('0' + result));
}