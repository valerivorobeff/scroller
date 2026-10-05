/**
 * @file server.h
 * @brief scroller server file
 */

#ifndef _SERVER_H_
#define _SERVER_H_

#include "gid.h"
#include <stddef.h>
#include <arpa/inet.h>

/* @todo: Move it to common information for scrs and scri */
#define SEQUENCE_HEADER_GID     0
#define SEQUENCE_DATA_GID       1
#define CLUSTER_HEADER_GID      2
#define CLUSTER_DATA_GID        3

#define DEFAULT_BACKLOG         10
#define DEFAULT_PORT            8081

#define DEFAULT_PAGECACHESZ0    8
#define DEFAULT_PAGECACHESZ1    8

#define DEFAULT_FDCACHESZ0      4
#define DEFAULT_FDCACHESZ1      4

#define DEFAULT_TRANSZ0         128
#define DEFAULT_TRANSZ1         128

/**
 * @brief Global interprocess shared page cache
 */
typedef struct PageCache PageCache;

extern PageCache *g_pagecache;

/**
 * @brief Global interprocess shared transaction cache
 */
typedef struct Tran Tran;

extern Tran *g_tran;

/**
 * @brief Server struct
 */
typedef struct Server {
    int argc;                   /**< Argument number - got from main() function */
    char **argv;                /**< Arguments (r/w) - got from main() function */
    int server_fd;              /**< Server socket descriptor */
    int backlog;                /**< Server backlog */
    in_port_t port;             /**< Server port */
    struct {
        GidPair sequence;       /**< Main seuence GidPair */
        GidPair cluster;        /**< Cluster table GidPair */
        GidPair user;           /**< User table GidPair */
        GidPair catalog;        /**< Catalog table GidPair */
        GidPair schema;         /**< Schema table GidPair */
        GidPair relation;       /**< Relation table GidPair */
        const char *encoding;   /**< Encoding */
        size_t pagecachesz[2];  /**< PageCache size - [0]: buckets, [1]: chains */
        size_t fdcachesz[2];    /**< File descriptor cache size - [0]: buckets, [1]: chains */
        size_t transz[2];       /**< Transaction cache size - [0]: buckets, [1]: chains */
    } system;
} Server;

/**
 * @brief Global server struct (not integprocess shared, just forked)
 */
extern Server g_server;

/**
 * @brief Initializes server
 * @note it uses g_server struct to store server information
 * @param path Path to cluster data
 * @param argc Argument number - got from main() function
   *param argv Arguments (r/w) - got from main() function
 * @return 0 - if succeed, error code otherwise
 */
int server_init(int argc, char *argv[]);

/**
 * @brief Runs server
 * @return 0 - if succeed, error code otherwise
 */
int server_run(void);

/**
 * @brief Drops server
 * @return 0 - if succeed, error code otherwise
 */
int server_drop(void);

#endif /* _SERVER_H_ */

