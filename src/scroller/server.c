#include "server.h"
#include "pagecache.h"
#include "sequence.h"
#include "server.h"
#include "cell.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdbool.h>

int server_init(const char *path);
int server_run();
int server_drop();

static bool directory_exists(const char *path);
size_t get_block_size(const char *fname);

PageCache *g_pagecache = NULL;

int
server_init(const char *path) {
    int result = directory_exists(path);

    if (result == false)
        fprintf(stderr, "Directory '%s' doesn't exist\n", path);
    else {
        Grid *hcluster;
        Grid *cluster;
        uint16_t name_idx;
        uint16_t value_idx;

        chdir(path);

        PAGESZ = get_block_size(path);

        printf("block size: %lu\n", PAGESZ);

        g_pages = aligned_alloc(PAGESZ, 16 * PAGESZ);
        g_fdcache = fdcache_create(g_fdcache, 4, 4, NULL);
        g_pagecache = pagecache_create(8, 8, NULL);

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

        printf("Cluster read\n");
    }

    return 0;
}

int
server_run() {
    return 0;
}

int
server_drop() {
    pagecache_free(g_pagecache);
    fdcache_free(g_fdcache);
    free(g_pages);

    return 0;
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

