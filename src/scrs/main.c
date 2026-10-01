#include "server.h"
#include <stdio.h>
#include <stdlib.h>

int
main(const int argc, char *argv[]) {
    int result;

    result = server_init(argc, argv);
    if (result)
        return result;

    result = server_run();
    if (result)
        return result;

    result = server_drop();

    return result;
}

