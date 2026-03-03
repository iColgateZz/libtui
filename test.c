#define PSH_BUILD_IMPL
#define LIBTUI_IMPL
    #include "libtui.h"

#define SEC(x)  x * 1000

// curl https://meowfacts.herokuapp.com/
typedef struct {
    u16 timer;
    Fd_Reader reader;
} Asker;

void main_loop();

i32 main(i32 argc, byte *argv[]) {
    PSH_REBUILD_UNITY_AUTO(argc, argv);

    init_terminal();
    set_max_timeout_ms(1);


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
    
}