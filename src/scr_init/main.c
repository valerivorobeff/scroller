#include "pagecache.h"
#include "sequence.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include <string.h>

PageCache *g_pagecache = NULL;

#define put_char(c, val, size)  strncpy(c, val, size - 1)

int
init_cluster(const char *path) {
    int result;

    result = mkdir(path, 0700);

    if (result) {
        switch (errno) {
            case EEXIST:
                fprintf(stderr, "Directory already exists\n");
                break;

            default:
                fprintf(stderr, "Error creating directory\n");
                break;
        }
    } else {
        Page hsequence;
        Page sequence;
        Page hcluster;
        Page cluster;
        int64_t currval;
        Row row;
        Column column;

        chdir(path);

        /*
         * Init sequence header
         */
        hsequence = pagecache_put_page(g_pagecache, 0);
        hsequence_init(hsequence);
        pagecache_flush(g_pagecache, 0);

        /*
         * Init main sequence
         */
        sequence = pagecache_put_page(g_pagecache, 1);
        sequence_init(hsequence, sequence, 0, 100, 2, 1, 0);
        pagecache_flush(g_pagecache, 1);

        /*
         * Init main cluster header
         */
        sequence_nextval(hsequence, sequence, &currval);

        hcluster = pagecache_put_page(g_pagecache, currval);
        hcluster = hgrid_init(hcluster, PAGESZ, GT_FIXED);
        hgrid_add_column(hcluster, "name", 32);
        hgrid_add_column(hcluster, "value", 32);

        pagecache_flush(g_pagecache, currval);

        /*
         * Init main cluster table
         */
        sequence_nextval(hsequence, sequence, &currval);

        cluster = pagecache_put_page(g_pagecache, currval);
        cluster = dgrid_init(cluster, PAGESZ, GT_FIXED, hcluster);

        row = dgrid_alloc_row(cluster);
        column = dgrid_get_column(hcluster, cluster, 0, 0);
        put_char(column, "Encoding", 32);

        column = dgrid_get_column(hcluster, cluster, 0, 1);
        put_char(column, "UTF-8", 32);

        pagecache_flush(g_pagecache, currval);

        printf("Cluster created\n");
    }

    return result;
}

ssize_t
get_block_size(const char *fname) {
    struct stat st;

    if (stat(fname ? fname : __FILE__, &st) == 0)
        return st.st_blksize;
    else
        return 1024;
}

int
main(const int argc, const char *argv[]) {
    int result;

    if (argc != 2) {
        fprintf(stderr, "usage: scr_init <PATH_TO_CLUSTER_HOME_DIR>\n");
        return EXIT_FAILURE;
    }

    PAGESZ = get_block_size(argv[0]);

    printf("block size: %lu\n", PAGESZ);

    g_pages = aligned_alloc(PAGESZ, 16 * PAGESZ);
    g_fdcache = fdcache_create(g_fdcache, 4, 4, NULL);
    g_pagecache = pagecache_create(8, 8, NULL);

    result = init_cluster(argv[1]);

    pagecache_free(g_pagecache);
    fdcache_free(g_fdcache);
    free(g_pages);

    return result;
}

