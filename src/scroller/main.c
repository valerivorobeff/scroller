#include "server.h"
#include <stdio.h>
#include <stdlib.h>

int
main(const int argc, const char *argv[]) {
    int result;
    Server server;

    if (argc != 2) {
        fprintf(stderr, "usage: scroller <PATH_TO_CLUSTER_HOME_DIR>\n");
        return EXIT_FAILURE;
    }

    result = server_init(argv[1], &server);
    result = server_run(&server);
    result = server_drop(&server);

    return result;
}

