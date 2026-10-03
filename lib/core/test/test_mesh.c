#include "quin.h"
#include "cell.h"
#include "mesh.h"
#include "pagecache.h"
#include "sequence.h"
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

const int64_t sequence_start = 4;
Page *g_sequence = NULL;
PageCache *g_pagecache = NULL;

const GidPair gp_sequence = {
    .header = { .full = 0 },
    .data = { .full = 1 }
};

Page hsequence;
Page sequence;

static const char *TEST_DIR = "/tmp/scroller/test/mesh/";

/* Helper function: create temporary test directory */
static int
create_test_dir(void) {
    int ret = 0;
    char *dir = strdup(TEST_DIR);
    char *prev = dir + 1;               /* Ignore first '/' */
    char *c;

    while (prev && *prev && (c = strchr(prev, '/')) != NULL) {
        *c = '\0';
        ret = mkdir(dir, 0755);

        if (ret == 0 || errno == EEXIST) {
            *c = '/';
            prev = c + 1;
            ret = 0;
        } else
            break;
    }

    if (ret == 0)
        ret = chdir(dir);

    free(dir);

    return ret;
}

/* Helper function: delete temporary test directory */
static int
remove_test_dir(void) {
    return rmdir(TEST_DIR);
}

static ssize_t
get_block_size(void) {
    struct stat st;

    if (stat(__FILE__, &st) == 0)
        return st.st_blksize;
    else
        return 4096;
}

static ssize_t
get_file_size(const char *fname) {
    struct stat st;

    if (stat(fname, &st) == 0)
        return st.st_size;
    else
        return -1;
}

int
init(void) {
    PAGESZ = get_block_size();

    g_pages = aligned_alloc(PAGESZ, 16 * PAGESZ);
    g_fdcache = fdcache_create(g_fdcache, 4, 4, NULL);
    g_pagecache = pagecache_create(8, 8, NULL);

    return !g_pages || !g_fdcache || !g_pagecache || create_test_dir();
}

int
done(void) {
    int ret = remove_test_dir();
    pagecache_free(g_pagecache);
    fdcache_free(g_fdcache);
    free(g_pages);

    return ret;
}

TEST(mesh)

    /**
     * @brief Test suite for mesh operations
     *
     */
    TEST_SUITE(init)

        TEST_CASE(init) {
            TEST_CHECK(init() == 0);
        }

        TEST_CASE(many_pages) {
            GidPair gp = {
                .header = { .parts = { .file_id = 2, .page = 0 } },
                .data = { .parts = { .file_id = 3, .page = 0 } }
            };

            Mitor m;

            /* Init sequence */
            hsequence = pagecache_put_page(g_pagecache, gp_sequence.header);
            TEST_CHECK(hsequence_init(hsequence) == 0);
            pagecache_flush(g_pagecache, gp_sequence.header);

            sequence = pagecache_put_page(g_pagecache, gp_sequence.data);
            TEST_CHECK(sequence_init(hsequence, sequence, 0, INT64_MAX, sequence_start, 1, 0) == 0);
            pagecache_flush(g_pagecache, gp_sequence.data);

            Grid *header = pagecache_put_page(g_pagecache, gp.header);
            Grid *data = pagecache_put_page(g_pagecache, gp.data);
            pagecache_flush(g_pagecache, gp.data);

            /* Init mesh header with 1 column of 1024 bytes */
            hmesh_init(header, PAGESZ, GT_FIXED);
            hmesh_add_column(header, "column", T_CHAR, 1024);
            pagecache_flush(g_pagecache, gp.header);

            /* Init mesh data */
            dmesh_init(data, PAGESZ, GT_FIXED, header);

            /* Insert rows */
            size_t expected = 1;   /* Expected number of pages in mesh */
            for (int i = 0; i != 10; ++i) {
                char buf[1024];
                Gid old = gp.data;
                m = mesh_alloc_row(&gp);
                snprintf(buf, sizeof(buf), "This is just a test string %i", i);
                mitor_put_datum(m, 0, make_char(&buf[0]));

                if (memcmp(&old, &gp.data, sizeof (Gid))) {
                    /* gids differ that means that one page was added with function mesh_alloc_row */
                    ++expected;
                    pagecache_flush(g_pagecache, old); /* Flush old page */
                }
            }

            /* Flush updated gp.data */
            pagecache_flush(g_pagecache, gp.data);

            ssize_t sz = get_file_size(gid2hex(gp.data).value);
            TEST_CHECK(sz / PAGESZ == expected);

            unlink(gid2hex(gp_sequence.header).value);
            unlink(gid2hex(gp_sequence.data).value);

            unlink(gid2hex(gp.header).value);
            unlink(gid2hex(gp.data).value);
        }

        TEST_CASE(drop) {
            TEST_CHECK(done() == 0);
        }

    TEST_SUITE_END()

TEST_END()  /* End of mesh test unit */

