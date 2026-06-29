#include "fdcache.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

void *fdcache_touch_fn(icache *cache, ssize_t key);

static int fdcache_open(ssize_t key);
static int fdcache_close(int fd);

/**
 * @brief Touch or create cache entry for given key
 * @param cache Cache handle
 * @param key Key to touch
 * @return Pointer to cache entry, or NULL on error
 */
void *
fdcache_touch_fn(icache *cache, ssize_t key) {
    const icache_idx_t idxoffs = cache->nodesz - sizeof(icache_idx_t);
    ihash *hash = icache_get_hash(cache);
    icache_idx_t *list = icache_get_list(cache);
    void *e = icache_get(cache, key);

    if (e != NULL)
        ilist2_move_front_by_idx(list, *(icache_idx_t *)(e + idxoffs));
    else {
        int fd = fdcache_open(key);
        if (fd == -1) {
            return NULL;
        }

        e = ihash_touch_fn(hash, key);

        if (e != NULL) {
            ((FdCache *)(e))->fd = fd;

            *(icache_idx_t *)(e + idxoffs) = ilist2_put_front(list, key);
            assert(*(icache_idx_t *)(e + idxoffs) != ILIST2_UNDEF);
        } else {
            const icache_idx_t lru_key = ilist2_pop_back(list);

            assert(lru_key != ILIST2_UNDEF);
            assert(ihash_exists(hash, lru_key));

            fdcache_close(ihash_get_member((FdCache *)hash, lru_key, fd));

            ihash_erase(hash, lru_key);

            e = ihash_touch_fn(hash, key);
            if (e == NULL) {
                fdcache_close(fd);
                return NULL;
            }

            ((FdCache *)(e))->fd = fd;

            *(icache_idx_t *)(e + idxoffs) = ilist2_put_front(list, key);
            assert(*(icache_idx_t *)(e + idxoffs) != ILIST2_UNDEF);
        }
    }

    return e;
}

/**
 * @brief Open file by key
 * @param key Key (Gid.full)
 * @return File descriptor, or -1 on error
 */
int
fdcache_open(ssize_t key) {
    Gid gid = { .full = (uint64_t)key };
    char buffer[sizeof(Gid) * 2 + 1];

    snprintf(buffer, sizeof(buffer), "%lX", (uint64_t)gid.parts.file_id);

#ifdef O_DIRECT
    return open(buffer, O_RDWR | O_CREAT | O_DIRECT, 0644);
#else
    return open(buffer, O_RDWR | O_CREAT | O_SYNC, 0644);
#endif
}

/**
 * @brief Close file descriptor
 * @param fd File descriptor
 * @return 0 on success, -1 on error
 */
int
fdcache_close(int fd) {
    return close(fd);
}

