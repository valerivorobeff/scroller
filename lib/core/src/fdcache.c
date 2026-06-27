#include "fdcache.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

FdCache *fdcache_touch_fn(FdCache *fc, Gid gid);

static int fdcache_open(Gid gid);
static int fdcache_close(int fd);

/**
 * @brief Inserts or updates an entry in the cache
 *
 * @param cache     Pointer to cache
 * @param key       Key to insert/update
 * @return          Pointer to entry (existing or new)
 *
 * @pre cache is properly initialized
 *
 * @retval non-NULL Pointer to the entry (inserted or updated)
 *
 * @note This function does NOT copy any value - only manages key
 * @note The value field is left untouched (caller must fill it)
 */
FdCache *
fdcache_touch_fn(FdCache *fc, Gid gid) {
    const icache *cache = (icache *)fc;
    const icache_idx_t idxoffs = cache->nodesz - sizeof(icache_idx_t);
    ihash *hash = icache_get_hash(cache);
    icache_idx_t *list = icache_get_list(cache);
    FdCache *e = (FdCache *)icache_get(cache, gid.full);

    if (e != NULL)
        ilist2_move_front_by_idx(list, *(icache_idx_t *)(e + idxoffs));
    else {
        int fd = fdcache_open(gid);
        if (fd == -1) {
            return NULL;
        }

        e = ihash_touch_fn(hash, gid.full);

        if (e != NULL) {
            e->fd = fd;

            *(icache_idx_t *)(e + idxoffs) = ilist2_put_front(list, gid.full);
            assert(*(icache_idx_t *)(e + idxoffs) != ILIST2_UNDEF);
        } else {
            const icache_idx_t lru_key = ilist2_pop_back(list);

            assert(lru_key != ILIST2_UNDEF);

            ihash_erase(hash, lru_key);

            e = ihash_touch_fn(hash, gid.full);

            fdcache_close(((FdCache *)lru_key)->fd);
            e->fd = fd;

            assert(e);
            *(icache_idx_t *)(e + idxoffs) = ilist2_put_front(list, gid.full);
            assert(*(icache_idx_t *)(e + idxoffs) != ILIST2_UNDEF);
        }
    }

    return e;
}

int
fdcache_open(Gid gid) {
    char buffer[sizeof(Gid) * 2 + 1];

    snprintf(buffer, sizeof(buffer), "%lX", (uint64_t)gid.parts.file_id);

#ifdef O_DIRECT
    return open(buffer, O_RDWR | O_CREAT | O_DIRECT, 0644);
#else
    return open(buffer, O_RDWR | O_CREAT | O_SYNC, 0644);
#endif
}

int
fdcache_close(int fd) {
    return close(fd);
}

