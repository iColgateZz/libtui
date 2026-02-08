#define PSH_BUILD_IMPL
    #include "psh_build/psh_build.h"

i32 main(i32 argc, byte *argv[]) {
    PSH_REBUILD_UNITY_AUTO(argc, argv);

    printf("Hello, world!\n");
    return 0;
}