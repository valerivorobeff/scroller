#include "tcp.h"
#include "worker.h"
#include "flog.h"
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

int tcp_init(Server *server);
int tcp_run(Server *server);
int tcp_destroy(Server *server);

int
tcp_init(Server *server) {
    const int opt = 1;
    const int backlog = 10;
    struct sockaddr_in server_addr;

    /* Initialize socket */
    server->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_fd < 0) {
        ferr("Cannot create socket");
        return 1;
    }

    /* Reuse socket option */
    if (setsockopt(server->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        ferr("Error in setsockopt");
        goto err_socket;
    }

    /* Set up server address */
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8081);

    /* Bind socket */
    if (bind(server->server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        ferr("Error in bind");
        goto err_socket;
    }

    /* Listen */
    if (listen(server->server_fd, backlog) < 0) {
        ferr("Error in listen");
        goto err_socket;
    }

    return 0;

    /*
     * Error handlers
     */
err_socket:
    close(server->server_fd);

    return 1;
}

int
tcp_run(Server *server) {
    int result = 0;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    for (;;) {
        char client_ip[INET_ADDRSTRLEN];

        server->client_fd = accept(server->server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (server->client_fd < 0) {
            if (errno == EINTR)
                continue; /* Interrupted with a signal */
            ferr("Error in accept");
            result = 1;

            break;
        }

        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        flog("[Parent] New connection from %s:%d\n",
               client_ip, ntohs(client_addr.sin_port));

        /* Make a new worker process */
        pid_t pid = fork();

        if (pid < 0) {
            ferr("Error in fork");
            close(server->client_fd);
            continue;
        }

        if (pid == 0) {
            /* Worker process */
            close(server->server_fd);   /* Close a copy of server process */
            server->server_fd = -1;     /* Undefine server_fd */
            result = worker_main(server);
            break;                      /* Return to main function */
        } else {
            /* Main process */
            close(server->client_fd);   /* Close client socket */
            server->client_fd = -1;      /* Undefine client_fd */
            printf("[Parent] Forked child PID: %d\n", pid);
            /* Back to accept() */
        }
    }

    return result;
}

int
tcp_destroy(Server *server) {
    if (server->server_fd == -1) {
        /* Worker process */
        return close(server->client_fd);
    } else if (server->client_fd == -1) {
        /* Main process */
        return close(server->server_fd);
    }

    return 0;
}

