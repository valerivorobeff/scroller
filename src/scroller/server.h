#ifndef _SERVER_H_
#define _SERVER_H_

#include "gid.h"

typedef struct PageCache PageCache;

extern PageCache *g_pagecache;

typedef struct Server {
    int server_fd;
    int client_fd;
    struct {
        GidPair sequence;
        GidPair cluster;
        GidPair user;
        GidPair catalog;
        const char *encoding;
    } system;
    const char *user;
} Server;

int server_init(const char *path, Server *server);
int server_run(Server *server);
int server_destroy(Server *server);

#endif /* _SERVER_H_ */

