/**
 * @file server.c
 * @brief scroller server file
 */

#include "server.h"
#include "tcp.h"
#include "pagecache.h"
#include "sequence.h"
#include "cell.h"
#include "table.h"
#include "flog.h"
#include "memory.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <assert.h>

int server_init(const char *path);
int server_run();
int server_drop();

static int caches_create();
static int caches_free();
static void sigchld_handler(int sig);
static bool directory_exists(const char *path);
size_t get_block_size(const char *fname);

PageCache *g_pagecache = NULL;
Server g_server;

int
server_init(const char *path) {
    Grid *hcluster;
    Grid *cluster;
    uint16_t name_idx;
    uint16_t string_idx;
    uint16_t header_idx;
    uint16_t data_idx;

    flog_init_default();

    if (!directory_exists(path))
        ffatal(1, "Directory '%s' doesn't exist", path);

    memory_init_default();

    chdir(path);

    PAGESZ = get_block_size(path);

    memset(&g_server, 0, sizeof(Server));

    /* Load default cache sizes */
    g_server.system.pagecachesz[0] = DEFAULT_PAGECACHESZ0;
    g_server.system.pagecachesz[1] = DEFAULT_PAGECACHESZ1;

    g_server.system.fdcachesz[0] = DEFAULT_FDCACHESZ0;
    g_server.system.fdcachesz[1] = DEFAULT_FDCACHESZ1;

    flog("block size: %lu\n", PAGESZ);

    if (caches_create()) /* Create caches with default sizes */
        return 1;

    /*
     * Init main cluster header
     */
    hcluster = pagecache_put_page(g_pagecache, 2);
    name_idx = htable_get_column_idx(hcluster, "name");
    assert(grid_idx_valid(name_idx));
    string_idx = htable_get_column_idx(hcluster, "string");
    assert(grid_idx_valid(string_idx));
    header_idx = htable_get_column_idx(hcluster, "header");
    assert(grid_idx_valid(header_idx));
    data_idx = htable_get_column_idx(hcluster, "data");
    assert(grid_idx_valid(data_idx));

    /*
     * Init main cluster table
     */
    cluster = pagecache_put_page(g_pagecache, 3);

    for (Titor i = titor_init(hcluster, cluster); titor_is_valid(i); titor_next(&i)) {
        const Datum name = titor_get_datum(i, name_idx);
        const Datum string = titor_get_datum(i, string_idx);
        const Datum header = titor_get_datum(i, header_idx);
        const Datum data = titor_get_datum(i, data_idx);

        if (eq_character(name, make_char("encoding"))) {
            g_server.system.encoding = sdup(string.value.character);
        } else if (eq_character(name, make_char("pagecache_size"))) {
            g_server.system.pagecachesz[0] = header.value.bigint;
            g_server.system.pagecachesz[1] = data.value.bigint;
        } else if (eq_character(name, make_char("fdcache_size"))) {
            g_server.system.fdcachesz[0] = header.value.bigint;
            g_server.system.fdcachesz[1] = data.value.bigint;
        } else if (eq_character(name, make_char("sequence"))) {
            g_server.system.sequence.header = (Gid){ .parts = { .file_id = header.value.bigint, .page = 0 }};
            g_server.system.sequence.data = (Gid){ .parts = { .file_id = data.value.bigint, .page = 0 }};
        } else if (eq_character(name, make_char("cluster"))) {
            g_server.system.cluster.header = (Gid){ .parts = { .file_id = header.value.bigint, .page = 0 }};
            g_server.system.cluster.data = (Gid){ .parts = { .file_id = data.value.bigint, .page = 0 }};
        } else if (eq_character(name, make_char("user"))) {
            g_server.system.user.header = (Gid){ .parts = { .file_id = header.value.bigint, .page = 0 }};
            g_server.system.user.data = (Gid){ .parts = { .file_id = data.value.bigint, .page = 0 }};
        } else if (eq_character(name, make_char("catalog"))) {
            g_server.system.catalog.header = (Gid){ .parts = { .file_id = header.value.bigint, .page = 0 }};
            g_server.system.catalog.data = (Gid){ .parts = { .file_id = data.value.bigint, .page = 0 }};
        } else if (eq_character(name, make_char("schema"))) {
            g_server.system.schema.header = (Gid){ .parts = { .file_id = header.value.bigint, .page = 0 }};
            g_server.system.schema.data = (Gid){ .parts = { .file_id = data.value.bigint, .page = 0 }};
        } else if (eq_character(name, make_char("relation"))) {
            g_server.system.relation.header = (Gid){ .parts = { .file_id = header.value.bigint, .page = 0 }};
            g_server.system.relation.data = (Gid){ .parts = { .file_id = data.value.bigint, .page = 0 }};
        } else {
            /* @todo: there is no way to show Datum as char *, I should make a function for it */
            ferr("Unknown parameter '%s' in cluster table", datum_sdup(name));
        }
    }

    assert(g_server.system.cluster.header.parts.file_id == 2);
    assert(g_server.system.cluster.data.parts.file_id == 3);

    /* Remake caches if only their sizes differ from default */
    if (g_server.system.pagecachesz[0] != DEFAULT_PAGECACHESZ0 ||
        g_server.system.pagecachesz[1] != DEFAULT_PAGECACHESZ1 ||
        g_server.system.fdcachesz[0] != DEFAULT_FDCACHESZ0 ||
        g_server.system.fdcachesz[1] != DEFAULT_FDCACHESZ1) {
        if (caches_free())
            ferr("Cannot free caches, continue with memory leak");

        if (caches_create()) /* Now that we have read the cache sizes from the cluster table we can remake them */
            return 1;
    }

    /* Initialize tcp */
    if (tcp_init())
        goto err;

    /* Set up SIGCHLD handler to clear zombies */
    signal(SIGCHLD, sigchld_handler);

    flog("Server started");

    return 0;

    /*
     * Error handlers
     */
err:
    if (caches_free())
        ferr("Cannot free caches, continue with memory leak");

    return 1;
}

int
server_run() {
   return tcp_run();
}

int
server_drop() {
    tcp_destroy();

    caches_free();

    flog("Server stopped");

    memory_destroy();

    return 0;
}

int
caches_create() {
    /* Create and initialize g_pages */
    g_pages = mmap(NULL,
        (g_server.system.pagecachesz[0] + g_server.system.pagecachesz[1]) * PAGESZ,
        PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    /* @todo: handle errno */
    if (g_pages == MAP_FAILED)
        ffatal(1, "Cannot allocate memory for pages");

    /* Create and initialize g_pagecache */
    g_pagecache = mmap(NULL,
        pagecache_get_required_memory_size(
            g_server.system.pagecachesz[0], g_server.system.pagecachesz[1]),
        PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    /* @todo: handle errno */
    if (g_pagecache == MAP_FAILED) {
        ferr("Cannot allocate memory for page cache");
        goto err_g_fdcache;
    }

    g_pagecache = pagecache_init(g_pagecache, 8, 8, NULL);
    if (g_pagecache == NULL) {
        ferr("Cannot initialize page cache");
        goto err_g_fdcache;
    }

    /* Create and initialize g_fdcache */
    g_fdcache = mmap(NULL,
        fdcache_get_required_memory_size(
            g_server.system.fdcachesz[0], g_server.system.fdcachesz[1]),
        PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    /* @todo: handle errno */
    if (g_fdcache == MAP_FAILED) {
        ferr("Cannot allocate memory for file descriptor cache");
        goto err_g_pages;
    }

    g_fdcache = fdcache_init(g_fdcache,
        g_server.system.fdcachesz[0], g_server.system.fdcachesz[1], NULL);
    if (g_fdcache == NULL) {
        ferr("Cannot create file descriptor cache");
        goto err_g_pages;
    }

    return 0;

    /*
     * Error handlers
     */
err_g_fdcache:
    munmap(g_fdcache,
        fdcache_get_required_memory_size(
            g_server.system.fdcachesz[0], g_server.system.fdcachesz[1]));

err_g_pages:
    free(g_pages);

    return 1;
}

int
caches_free() {
    int ret = 0;

    ret |= munmap(g_fdcache,
        fdcache_get_required_memory_size(
            g_server.system.fdcachesz[0], g_server.system.fdcachesz[1]));

    ret |= munmap(g_pagecache,
        pagecache_get_required_memory_size(
            g_server.system.pagecachesz[0], g_server.system.pagecachesz[1]));

    ret |= munmap(g_pages,
        (g_server.system.pagecachesz[0] + g_server.system.pagecachesz[1]) * PAGESZ);

    return ret;
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
        return 1024; /* @todo: Hardcode is not good */
}

