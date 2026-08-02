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
#include <sys/stat.h>
#include <sys/wait.h>
#include <assert.h>

int server_init(const char *path, Server *server);
int server_run(Server *server);
int server_destroy(Server *server);

static void sigchld_handler(int sig);
static bool directory_exists(const char *path);
size_t get_block_size(const char *fname);

PageCache *g_pagecache = NULL;

int
server_init(const char *path, Server *server) {
    size_t pagecachen;
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

    memset(server, 0, sizeof(Server));

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
            server->system.encoding = sdup(string.value.character);
        } else if (eq_character(name, make_char("sequence"))) {
            server->system.sequence.header = (Gid){ .parts = { .file_id = header.value.bigint, .page = 0 }};
            server->system.sequence.data = (Gid){ .parts = { .file_id = data.value.bigint, .page = 0 }};
        } else if (eq_character(name, make_char("cluster"))) {
            server->system.cluster.header = (Gid){ .parts = { .file_id = header.value.bigint, .page = 0 }};
            server->system.cluster.data = (Gid){ .parts = { .file_id = data.value.bigint, .page = 0 }};
        } else if (eq_character(name, make_char("user"))) {
            server->system.user.header = (Gid){ .parts = { .file_id = header.value.bigint, .page = 0 }};
            server->system.user.data = (Gid){ .parts = { .file_id = data.value.bigint, .page = 0 }};
        } else if (eq_character(name, make_char("catalog"))) {
            server->system.catalog.header = (Gid){ .parts = { .file_id = header.value.bigint, .page = 0 }};
            server->system.catalog.data = (Gid){ .parts = { .file_id = data.value.bigint, .page = 0 }};
        } else if (eq_character(name, make_char("schema"))) {
            server->system.schema.header = (Gid){ .parts = { .file_id = header.value.bigint, .page = 0 }};
            server->system.schema.data = (Gid){ .parts = { .file_id = data.value.bigint, .page = 0 }};
        } else if (eq_character(name, make_char("relation"))) {
            server->system.relation.header = (Gid){ .parts = { .file_id = header.value.bigint, .page = 0 }};
            server->system.relation.data = (Gid){ .parts = { .file_id = data.value.bigint, .page = 0 }};
        } else {
            /* @todo: there is no way to show Datum as char *, I should make a function for it */
            ferr("Unknown parameter in cluster table");
        }
    }

    assert(server->system.cluster.header.parts.file_id == 2);
    assert(server->system.cluster.data.parts.file_id == 3);

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
server_destroy(Server *server) {
    tcp_destroy(server);

    pagecache_free(g_pagecache);
    fdcache_free(g_fdcache);
    free(g_pages);

    flog("Server stopped");

    memory_destroy();

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

