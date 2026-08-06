#include "server.h"
#include <stdio.h>
#include <stdlib.h>

int
main(const int argc, const char *argv[]) {
    int result;

    if (argc != 2) {
        fprintf(stderr, "usage: scroller <PATH_TO_CLUSTER_HOME_DIR>\n");
        return EXIT_FAILURE;
    }

    result = server_init(argv[1]);
    if (result)
        return result;

    result = server_run();
    result &= server_drop();

    return result;
}

