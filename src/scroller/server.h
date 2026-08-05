#ifndef _SERVER_H_
#define _SERVER_H_

#include "gid.h"

typedef struct PageCache PageCache;

extern PageCache *g_pagecache;

typedef struct Server {
    int server_fd;
    struct {
        GidPair sequence;
        GidPair cluster;
        GidPair user;
        GidPair catalog;
        GidPair schema;
        GidPair relation;
        const char *encoding;
    } system;
} Server;

extern Server g_server;

int server_init(const char *path);
int server_run();
int server_drop();

#endif /* _SERVER_H_ */

