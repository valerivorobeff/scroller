#include "pagecache.h"
#include "sequence.h"
#include "cell.h"
#include "table.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>

PageCache *g_pagecache = NULL;

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
        Gid gid_cluster;
        Page hsequence;
        Page sequence;
        Page hcluster;
        Page cluster;
        Page huser;
        Page user;
        Page hcatalog;
        Page catalog;
        int64_t prevval;
        int64_t currval;
        Titor row;
        Cell cell;

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
        sequence_init(hsequence, sequence, 0, INT64_MAX, 2, 1, 0);

        /*
         * Init main cluster header
         */
        sequence_nextval(hsequence, sequence, &currval);

        hcluster = pagecache_put_page(g_pagecache, currval);
        hcluster = htable_init(hcluster, PAGESZ, GT_FIXED);
        htable_add_column(hcluster, "name", T_CHAR, 32);
        htable_add_column(hcluster, "string", T_CHAR, 32);
        htable_add_column(hcluster, "header", T_BIGINT, 32);
        htable_add_column(hcluster, "data", T_BIGINT, 32);

        pagecache_flush(g_pagecache, currval);

        /*
         * Init main cluster table
         */
        sequence_nextval(hsequence, sequence, &currval);
        gid_cluster = (Gid){ .parts = { .file_id = currval, .page = 0 }};

        cluster = pagecache_put_page(g_pagecache, currval);
        cluster = dtable_init(cluster, PAGESZ, GT_FIXED, hcluster);

        row = table_alloc_row(hcluster, cluster);
        cell = table_get_cell(row, 0);
        put_char(cell, "Encoding", 32);

        cell = table_get_cell(row, 1);
        put_char(cell, "UTF-8", 32);

        /* Flush cluster table not now but in the end of initialization */

        /*
         * Init user header
         */
        sequence_nextval(hsequence, sequence, &currval);
        prevval = currval;

        huser = pagecache_put_page(g_pagecache, currval);
        huser = htable_init(huser, PAGESZ, GT_FIXED);
        htable_add_column(huser, "name", T_CHAR, 32);

        pagecache_flush(g_pagecache, currval);

        /*
         * Init user table
         */
        sequence_nextval(hsequence, sequence, &currval);

        user = pagecache_put_page(g_pagecache, currval);
        user = dtable_init(user, PAGESZ, GT_FIXED, huser);

        row = table_alloc_row(huser, user);
        cell = table_get_cell(row, 0);
        put_char(cell, "scroller", 32);

        pagecache_flush(g_pagecache, currval);

        /* Add user table Gids to cluster table */
        row = table_alloc_row(hcluster, cluster);
        cell = table_get_cell(row, 0);
        put_char(cell, "user", 32);

        cell = table_get_cell(row, 2);
        put_bigint(cell, prevval);

        cell = table_get_cell(row, 3);
        put_bigint(cell, currval);

        /*
         * Init catalog header
         */
        sequence_nextval(hsequence, sequence, &currval);
        prevval = currval;

        hcatalog = pagecache_put_page(g_pagecache, currval);
        hcatalog = htable_init(hcatalog, PAGESZ, GT_FIXED);
        htable_add_column(hcatalog, "name", T_CHAR, 32);

        pagecache_flush(g_pagecache, currval);

        /*
         * Init catalog table
         */
        sequence_nextval(hsequence, sequence, &currval);

        catalog = pagecache_put_page(g_pagecache, currval);
        catalog = dtable_init(catalog, PAGESZ, GT_FIXED, hcatalog);

        pagecache_flush(g_pagecache, currval);

        /* Add catalog table Gids to cluster table */
        row = table_alloc_row(hcluster, cluster);
        cell = table_get_cell(row, 0);
        put_char(cell, "catalog", 32);

        cell = table_get_cell(row, 2);
        put_bigint(cell, prevval);

        cell = table_get_cell(row, 3);
        put_bigint(cell, currval);

        pagecache_flush(g_pagecache, gid_cluster.parts.file_id); /* Flush cluster table */
        pagecache_flush(g_pagecache, 1); /* Flush main sequence */

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

