#include "server.h"
#include "pagecache.h"
#include "sequence.h"
#include "server.h"
#include "cell.h"
#include "flog.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>

int server_init(const char *path, Server *server);
int server_run(Server *server);
int server_drop(Server *server);

static void sigchld_handler(int sig);
static bool directory_exists(const char *path);
size_t get_block_size(const char *fname);

static int tcp_init(Server *server);
static int tcp_run(Server *server);
static int tcp_drop(Server *server);

int worker_main(Server *server);

PageCache *g_pagecache = NULL;

int
server_init(const char *path, Server *server) {
    size_t pagecachen;
    Grid *hcluster;
    Grid *cluster;
    uint16_t name_idx;
    uint16_t value_idx;

    flog_init_default();

    if (!directory_exists(path))
        ffatal(1, "Directory '%s' doesn't exist", path);

    chdir(path);

    PAGESZ = get_block_size(path);

    flog("block size: %lu\n", PAGESZ);

    g_pages = aligned_alloc(PAGESZ, 16 * PAGESZ);           /* Initialize g_pages */
    if (g_pages == NULL)
        ffatal(1, "Cannot allocate memory for pages");

    g_fdcache = fdcache_create(g_fdcache, 4, 4, NULL);      /* Initialize g_fdcache */
    if (g_fdcache == NULL) {
        ferr("Cannot create file descriptor cache");
        goto err_g_pages;
    }

    pagecachen = pagecache_get_required_memory_size(8, 8);  /* Initialize g_pagecache */
    pagecachen = (pagecachen % PAGESZ) ?
        pagecachen / PAGESZ + 1 : pagecachen / PAGESZ;
    g_pagecache = aligned_alloc(PAGESZ, pagecachen * PAGESZ);
    if (g_pagecache == NULL) {
        ferr("Cannot allocate memory for page cache");
        goto err_g_fdcache;
    }

    g_pagecache = pagecache_init(g_pagecache, 8, 8, NULL);
    if (g_pagecache == NULL) {
        ferr("Cannot initialize page cache");
        goto err_g_fdcache;
    }

    /*
     * Init main cluster header
     */
    hcluster = pagecache_put_page(g_pagecache, 2);
    name_idx = hgrid_get_column_idx(hcluster, "name");
    assert(grid_idx_valid(name_idx));
    value_idx = hgrid_get_column_idx(hcluster, "value");
    assert(grid_idx_valid(value_idx));

    /*
     * Init main cluster table
     */
    cluster = pagecache_put_page(g_pagecache, 3);

    for (int i = 0; i; ++i) {
        Datum name = dgrid_get_datum(hcluster, cluster, i, name_idx);
        Datum value = dgrid_get_datum(hcluster, cluster, i, value_idx);
    }

    tcp_init(server);

    /* Set up SIGCHLD handler to clear zombies */
    signal(SIGCHLD, sigchld_handler);

    flog("Server started");

    return 0;

    /*
     * Error handlers
     */
err_g_fdcache:
    fdcache_free(g_fdcache);

err_g_pages:
    free(g_pages);

    return 1;
}

int
server_run(Server *server) {
   return tcp_run(server);
}

int
server_drop(Server *server) {
    tcp_drop(server);

    pagecache_free(g_pagecache);
    fdcache_free(g_fdcache);
    free(g_pages);

    flog("Server stopped");

    return 0;
}

/* SIGCHLD Handler to clean zombies */
void
sigchld_handler(int sig) {
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

bool
directory_exists(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0)
        return S_ISDIR(st.st_mode);
    else
        return false;
}

size_t
get_block_size(const char *fname) {
    struct stat st;

    if (stat(fname ? fname : __FILE__, &st) == 0)
        return st.st_blksize;
    else
        return 1024;
}

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
            exit(worker_main(server));
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
tcp_drop(Server *server) {
    if (server->server_fd == -1) {
        /* Worker process */
        return close(server->client_fd);
    } else if (server->client_fd == -1) {
        /* Main process */
        return close(server->server_fd);
    }

    return 0;
}

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

