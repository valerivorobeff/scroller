#include "worker.h"
#include "flog.h"
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

int worker_main(Server *server);

int
worker_main(Server *server) {
    char buffer[1024];
    const int client_fd = server->client_fd;
    ssize_t bytes_read;

    while ((bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        flog("[Child %d] Received: %s", getpid(), buffer);

        /* Send back */
        send(client_fd, "Answer: ", 8, 0);
        send(client_fd, buffer, bytes_read, 0);

        if (memcmp(buffer, "exit", 4) == 0) {
            send(client_fd, "Bye!", 4, 0);
            break;
        }
    }

    if (bytes_read == 0) {
        flog("[Child %d] Client disconnected\n", getpid());
    } else if (bytes_read < 0) {
        flog("recv");
    }

    /* Drop worker */
    close(client_fd);

    return 0;
}

