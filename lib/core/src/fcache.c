#include "fcache.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

void *fcache_touch_fn(icache *cache, ssize_t key);
static int fcache_open(ssize_t key);
static int fcache_close(int fd);

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
void *
fcache_touch_fn(icache *cache, ssize_t key) {
    const icache_idx_t idxoffs = cache->nodesz - sizeof(icache_idx_t);
    ihash *hash = icache_get_hash(cache);
    icache_idx_t *list = icache_get_list(cache);
    void *e = icache_get(cache, key);

    if (e != NULL)
        ilist2_move_front_by_idx(list, *(icache_idx_t *)(e + idxoffs));
    else {
        int fd = fcache_open(key);
        if (fd == -1) {
            return NULL;
        }

        e = ihash_touch_fn(hash, key);

        if (e != NULL) {
            ((fcache *)e)->fd = fd;

            *(icache_idx_t *)(e + idxoffs) = ilist2_put_front(list, key);
            assert(*(icache_idx_t *)(e + idxoffs) != ILIST2_UNDEF);
        } else {
            icache_idx_t lru_key = ilist2_pop_back(list);

            assert(lru_key != ILIST2_UNDEF);

            ihash_erase(hash, lru_key);

            e = ihash_touch_fn(hash, key);

            fcache_close(((fcache *)lru_key)->fd);
            ((fcache *)e)->fd = fd;

            assert(e);
            *(icache_idx_t *)(e + idxoffs) = ilist2_put_front(list, key);
            assert(*(icache_idx_t *)(e + idxoffs) != ILIST2_UNDEF);
        }
    }

    return e;
}

int
fcache_open(ssize_t key) {
    Gid gid;
    char buffer[sizeof(Gid) * 2 + 1];

    gid.full = key;
    snprintf(buffer, sizeof(buffer), "%lX", (uint64_t)gid.parts.file_id);
    return open(buffer, O_CREAT 
                  /* mode_t mode */ );
}

int
fcache_close(int fd) {
    return close(fd);
}

