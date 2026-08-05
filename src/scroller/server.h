#ifndef _SERVER_H_
#define _SERVER_H_

#include "gid.h"
#include <stddef.h>

/* @todo: Move it to common information for scroller and scr_init */
#define DEFAULT_PAGECACHESZ0    8
#define DEFAULT_PAGECACHESZ1    8

#define DEFAULT_FDCACHESZ0      4
#define DEFAULT_FDCACHESZ1      4

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
        size_t pagecachesz[2];
        size_t fdcachesz[2];
    } system;
} Server;

extern Server g_server;

int server_init(const char *path);
int server_run();
int server_drop();

#endif /* _SERVER_H_ */

