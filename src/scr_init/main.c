#include "pagecache.h"
#include "sequence.h"
#include "cell.h"
#include "table.h"
#include "../scroller/server.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>

void add_gid_pair(Grid *hcluster, Grid *cluster, const char *name, GidPair gidp);

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
        GidPair gp_sequence = {
            .header = { .parts = { .file_id = 0, .page = 0 }},
            .data = { .parts = { .file_id = 1, .page = 0 }}
        };

        GidPair gp_cluster;
        GidPair gp_user;
        GidPair gp_catalog;
        GidPair gp_schema;
        GidPair gp_relation;
        Page hsequence;
        Page sequence;
        Page hcluster;
        Page cluster;
        Page huser;
        Page user;
        Page hcatalog;
        Page catalog;
        Page hschema;
        Page schema;
        Page hrelation;
        Page relation;
        int64_t currval;
        uint16_t name_idx;
        uint16_t string_idx;

        Titor row;
        Cell cell;

        chdir(path);

        /**********************************************************************
         *
         * Sequence
         *
         *********************************************************************/

        /*
         * Init sequence header
         */
        hsequence = pagecache_put_page(g_pagecache, gp_sequence.header.full);
        hsequence_init(hsequence);
        pagecache_flush(g_pagecache, gp_sequence.header.full);

        /*
         * Init main sequence
         */
        sequence = pagecache_put_page(g_pagecache, gp_sequence.data.full);
        sequence_init(hsequence, sequence, 0, INT64_MAX, gp_sequence.data.full + 1, 1, 0);

        /**********************************************************************
         *
         * Cluster
         *
         *********************************************************************/

        /*
         * Init main cluster header
         */
        sequence_nextval(hsequence, sequence, &currval);
        gp_cluster.header = (Gid){ .parts = { .file_id = currval, .page = 0 }};

        hcluster = pagecache_put_page(g_pagecache, currval);
        hcluster = htable_init(hcluster, PAGESZ, GT_FIXED);
        htable_add_column(hcluster, "name", T_CHAR, 32);
        htable_add_column(hcluster, "string", T_CHAR, 32);
        htable_add_column(hcluster, "header", T_BIGINT, 0);
        htable_add_column(hcluster, "data", T_BIGINT, 0);

        name_idx = htable_get_column_idx(hcluster, "name");
        assert(grid_idx_valid(name_idx));
        string_idx = htable_get_column_idx(hcluster, "string");
        assert(grid_idx_valid(string_idx));

        pagecache_flush(g_pagecache, currval);

        /*
         * Init main cluster table
         */
        sequence_nextval(hsequence, sequence, &currval);
        gp_cluster.data = (Gid){ .parts = { .file_id = currval, .page = 0 }};

        cluster = pagecache_put_page(g_pagecache, currval);
        cluster = dtable_init(cluster, PAGESZ, GT_FIXED, hcluster);

        row = table_alloc_row(hcluster, cluster);
        cell = table_get_cell(row, name_idx);
        put_char(cell, "encoding", 32);

        cell = table_get_cell(row, string_idx);
        put_char(cell, "UTF-8", 32);

        /* Add pagecache_size cluster table */
        add_gid_pair(hcluster, cluster, "pagecache_size",
            (GidPair){ .header.full = DEFAULT_PAGECACHESZ0, .data.full =  DEFAULT_PAGECACHESZ1 }
        );

        /* Add fdcache_size cluster table */
        add_gid_pair(hcluster, cluster, "fdcache_size",
            (GidPair){ .header.full = DEFAULT_FDCACHESZ0, .data.full = DEFAULT_FDCACHESZ1 }
        );

        /* Add main sequence GidPair to cluster table */
        add_gid_pair(hcluster, cluster, "sequence", gp_sequence);

        /* Add cluster table GidPair to cluster table */
        add_gid_pair(hcluster, cluster, "cluster", gp_cluster);

        /* Flush cluster table not now but in the end of initialization */

        /**********************************************************************
         *
         * User
         *
         *********************************************************************/

        /*
         * Init user header
         */
        sequence_nextval(hsequence, sequence, &currval);
        gp_user.header = (Gid){ .parts = { .file_id = currval, .page = 0 }};

        huser = pagecache_put_page(g_pagecache, currval);
        huser = htable_init(huser, PAGESZ, GT_FIXED);
        htable_add_column(huser, "name", T_CHAR, 32);

        pagecache_flush(g_pagecache, currval);

        /*
         * Init user table
         */
        sequence_nextval(hsequence, sequence, &currval);
        gp_user.data = (Gid){ .parts = { .file_id = currval, .page = 0 }};

        user = pagecache_put_page(g_pagecache, currval);
        user = dtable_init(user, PAGESZ, GT_FIXED, huser);

        row = table_alloc_row(huser, user);
        cell = table_get_cell(row, 0);
        put_char(cell, "scroller", 32);

        pagecache_flush(g_pagecache, currval);

        /* Add user table GidPair to cluster table */
        add_gid_pair(hcluster, cluster, "user", gp_user);

        /**********************************************************************
         *
         * Catalog
         *
         *********************************************************************/

        /*
         * Init catalog header
         */
        sequence_nextval(hsequence, sequence, &currval);
        gp_catalog.header = (Gid){ .parts = { .file_id = currval, .page = 0 }};

        hcatalog = pagecache_put_page(g_pagecache, currval);
        hcatalog = htable_init(hcatalog, PAGESZ, GT_FIXED);
        htable_add_column(hcatalog, "name", T_CHAR, 32);

        pagecache_flush(g_pagecache, currval);

        /*
         * Init catalog table
         */
        sequence_nextval(hsequence, sequence, &currval);
        gp_catalog.data = (Gid){ .parts = { .file_id = currval, .page = 0 }};

        catalog = pagecache_put_page(g_pagecache, currval);
        catalog = dtable_init(catalog, PAGESZ, GT_FIXED, hcatalog);

        pagecache_flush(g_pagecache, currval);

        /* Add catalog table GidPair to cluster table */
        add_gid_pair(hcluster, cluster, "catalog", gp_catalog);

        /**********************************************************************
         *
         * Schema
         *
         *********************************************************************/

        /*
         * Init schema header
         */
        sequence_nextval(hsequence, sequence, &currval);
        gp_schema.header = (Gid){ .parts = { .file_id = currval, .page = 0 }};

        hschema = pagecache_put_page(g_pagecache, currval);
        hschema = htable_init(hschema, PAGESZ, GT_FIXED);
        htable_add_column(hschema, "catalog", T_CHAR, 32);
        htable_add_column(hschema, "schema", T_CHAR, 32);

        pagecache_flush(g_pagecache, currval);

        /*
         * Init schema table
         */
        sequence_nextval(hsequence, sequence, &currval);
        gp_schema.data = (Gid){ .parts = { .file_id = currval, .page = 0 }};

        schema = pagecache_put_page(g_pagecache, currval);
        schema = dtable_init(schema, PAGESZ, GT_FIXED, hschema);

        pagecache_flush(g_pagecache, currval);

        /* Add schema table GidPair to cluster table */
        add_gid_pair(hcluster, cluster, "schema", gp_schema);

        /**********************************************************************
         *
         * Relation
         *
         *********************************************************************/

        /*
         * Init relation header
         */
        sequence_nextval(hsequence, sequence, &currval);
        gp_relation.header = (Gid){ .parts = { .file_id = currval, .page = 0 }};

        hrelation = pagecache_put_page(g_pagecache, currval);
        hrelation = htable_init(hrelation, PAGESZ, GT_FIXED);
        htable_add_column(hrelation, "catalog", T_CHAR, 32);
        htable_add_column(hrelation, "schema", T_CHAR, 32);
        htable_add_column(hrelation, "relation", T_CHAR, 32);
        htable_add_column(hrelation, "header_gid", T_BIGINT, 0);
        htable_add_column(hrelation, "data_gid", T_BIGINT, 0);

        pagecache_flush(g_pagecache, currval);

        /*
         * Init relation table
         */
        sequence_nextval(hsequence, sequence, &currval);
        gp_relation.data = (Gid){ .parts = { .file_id = currval, .page = 0 }};

        relation = pagecache_put_page(g_pagecache, currval);
        relation = dtable_init(relation, PAGESZ, GT_FIXED, hrelation);

        pagecache_flush(g_pagecache, currval);

        /* Add relation table GidPair to cluster table */
        add_gid_pair(hcluster, cluster, "relation", gp_relation);

        /**********************************************************************
         *
         * Flush cluster an sequence
         *
         *********************************************************************/

        pagecache_flush(g_pagecache, gp_cluster.data.full); /* Flush cluster table */
        pagecache_flush(g_pagecache, gp_sequence.data.full); /* Flush main sequence */

        printf("Cluster created\n");
    }

    return result;
}

/** Add table GidPair to cluster table */
void
add_gid_pair(Grid *hcluster, Grid *cluster, const char *name, GidPair gidp) {
    uint16_t name_idx = htable_get_column_idx(hcluster, "name");
    uint16_t header_idx = htable_get_column_idx(hcluster, "header");
    uint16_t data_idx = htable_get_column_idx(hcluster, "data");

    Titor row = table_alloc_row(hcluster, cluster);
    Cell cell;

    assert(grid_idx_valid(name_idx));
    assert(grid_idx_valid(header_idx));
    assert(grid_idx_valid(data_idx));

    cell = table_get_cell(row, name_idx);
    put_char(cell, name, 32);

    cell = table_get_cell(row, header_idx);
    put_bigint(cell, gidp.header.full);

    cell = table_get_cell(row, data_idx);
    put_bigint(cell, gidp.data.full);
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

