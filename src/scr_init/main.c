#include "grid.h"
#include "fdcache.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

int
init_cluster(const char *path) {
    int result;

    result = mkdir(path, 0700);

    if (result) {
        switch (errno) {
            case EEXIST:
                result = 0;
                break;

            default:
                fprintf(stderr, "Error creating directory\n");
                break;
        }
    } else {
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
    Page page_pool;
    FdCache *fdcache;

    if (argc != 2) {
        fprintf(stderr, "usage: scr_init <PATH_TO_CLUSTER_HOME_DIR>\n");
        return EXIT_FAILURE;
    }

    result = init_cluster(argv[1]);
    PAGESZ = get_block_size(argv[1]);

    printf("block size: %lu\n", PAGESZ);

    page_pool = aligned_alloc(PAGESZ, 8 * PAGESZ);
    fdcache = fdcache_create(fdcache, 4, 4, NULL);

    fdcache_free(fdcache);
    free(page_pool);

    return result;
}

