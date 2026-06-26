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
block_size(const char *fname) {
    struct stat st;

    if (stat(fname, &st) == 0)
        return st.st_blksize;
    else
        return -1;
}

int
main(const int argc, const char *argv[]) {
    int result;
    ssize_t blocksz;
    Page page_pool;
    FdCache *fdcache;

    if (argc != 2) {
        fprintf(stderr, "usage: scr_init <PATH_TO_CLUSTER_HOME_DIR>\n");
        return EXIT_FAILURE;
    }

    result = init_cluster(argv[1]);
    blocksz = block_size(argv[1]);

    printf("block size: %lu\n", blocksz);

    page_pool = aligned_alloc(blocksz, 8 * blocksz);
    fdcache = fdcache_create(fdcache, 4, 4, NULL);

    fdcache_free(fdcache);
    free(page_pool);

    return result;
}

