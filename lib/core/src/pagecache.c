#include "pagecache.h"
#include <unistd.h>
#include <malloc.h>
#include <assert.h>

Page g_pages = NULL;
FdCache *g_fdcache = NULL;

PageCache *pagecache_create_fn(size_t bucketsz, size_t chainsz, ihash_hash_fn hash_fn);
PageCache *pagecache_init_fn(void *p, size_t bucketsz, size_t chainsz, ihash_hash_fn hash_fn);
void pagecache_clear(void *p, ihash_hash_fn hash_fn);
void *pagecache_touch_fn(icache *cache, ssize_t key);
ssize_t pagecache_flush(PageCache *cache, ssize_t key);

static pagecache_idx_t pagecache_read(icache *cache, ssize_t key);
static pagecache_idx_t pagecache_write(icache *cache, ssize_t key);

PageCache *
pagecache_create_fn(size_t bucketsz, size_t chainsz, ihash_hash_fn hash_fn) {
    icache *cache;

    if (bucketsz == 0)
        return NULL;    /* Error, impossible to init a hash without buckets */

    /* Allocate contiguous memory */
    cache = malloc(icache_get_required_memory_size(bucketsz, chainsz, sizeof(PageCache),
        pagecache_get_extra_size(bucketsz + chainsz)));

    return cache ? pagecache_init_fn(cache, bucketsz, chainsz, hash_fn) : NULL;
}

PageCache *
pagecache_init_fn(void *p, size_t bucketsz, size_t chainsz, ihash_hash_fn hash_fn) {
    const size_t fullsz = bucketsz + chainsz;
    icache *cache = (icache *)icache_init((PageCache *)p, bucketsz, chainsz, hash_fn, pagecache_get_extra_size(fullsz)); 
    pagecache_idx_t *page_idx_stack;

    if (cache == NULL)
        return NULL;

    page_idx_stack = icache_get_extra(cache);
    page_idx_stack = ilist2_init(page_idx_stack, fullsz);

    if (page_idx_stack == NULL)
        return NULL;

    for (pagecache_idx_t i = fullsz - 1; i >= 0; --i)
        ilist2_put_back(page_idx_stack, i);

    return (PageCache *)cache;
}

void
pagecache_clear(void *p, ihash_hash_fn hash_fn) {
    icache *cache = p;
    ihash *hash = icache_get_hash(cache);
    const size_t fullsz = hash->bucketsz + hash->chainsz;
    pagecache_idx_t *page_idx_stack = icache_get_extra(cache);

    icache_clear(cache, hash_fn);
    ilist2_clear(page_idx_stack);

    for (pagecache_idx_t i = fullsz - 1; i >= 0; --i)
        ilist2_put_back(page_idx_stack, i);
}

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
pagecache_touch_fn(icache *cache, ssize_t key) {
    const icache_idx_t idxoffs = cache->nodesz - sizeof(icache_idx_t);
    ihash *hash = icache_get_hash(cache);
    icache_idx_t *lru_list = icache_get_list(cache);
    void *e = icache_get(cache, key);

    if (e != NULL)
                                                            /* Make node mru */
        ilist2_move_front_by_idx(lru_list, *(icache_idx_t *)(e + idxoffs));
    else {
        e = ihash_touch_fn(hash, key);

        if (e == NULL) {
            ilist2_idx_t lru_key = ilist2_pop_back(lru_list); /* Pop lru key from lru list */
            assert(lru_key != ILIST2_UNDEF);

            pagecache_write(cache, lru_key);                /* Save lru page to disk */
            ihash_erase(hash, lru_key);                     /* Erase it from hash */

            e = ihash_touch_fn(hash, key);                  /* Put new node to hash */
            assert(e);
        }

        ((PageCache *)(e))->page_idx = pagecache_read(cache, key);
        assert((pagecache_idx_t)((PageCache *)(e))->page_idx != PAGECACHE_UNDEF);

                                                            /* Put lru list node's idx into hash */
        *(icache_idx_t *)(e + idxoffs) = ilist2_put_front(lru_list, key);
        assert(*(icache_idx_t *)(e + idxoffs) != ILIST2_UNDEF);
    }

    return e;
}

ssize_t
pagecache_flush(PageCache *cache, ssize_t key) {
    const FdCache *e = fdcache_put(g_fdcache, key);
    const size_t *idx = icache_get_member_ptr(cache, key, page_idx);
    ssize_t written_bytes;

    assert(e != NULL);
    assert(idx != NULL);

    written_bytes = write(e->fd, g_pages + *idx * PAGESZ, PAGESZ);

    return written_bytes != (ssize_t)PAGESZ ? written_bytes : 0;
}

pagecache_idx_t
pagecache_read(icache *cache, ssize_t key) {
    const pagecache_idx_t *page_idx_stack = icache_get_extra(cache);
    const pagecache_idx_t cur_idx = ilist2_pop_back(page_idx_stack);
    const FdCache *e = fdcache_put(g_fdcache, key);
    ssize_t read_bytes;

    assert(cur_idx != ILIST2_UNDEF);
    assert(e != NULL);

    read_bytes = read(e->fd, g_pages + cur_idx * PAGESZ, PAGESZ);
    if (read_bytes == -1) {
        /* @todo */
        return PAGECACHE_UNDEF;
    }

    /* We shouldn't check read_bytes == PAGESZ here because
     * the file can be new and empty */

    return cur_idx;
}

pagecache_idx_t
pagecache_write(icache *cache, ssize_t key) {
    pagecache_idx_t *page_idx_stack = icache_get_extra(cache);
    pagecache_idx_t cur_idx;
    const FdCache *e = fdcache_put(g_fdcache, key);
    const size_t *idx = icache_get_member_ptr((PageCache *)cache, key, page_idx);
    ssize_t written_bytes;

    assert(e != NULL);
    assert(idx != NULL);

    written_bytes = write(e->fd, g_pages + *idx * PAGESZ, PAGESZ);
    if (written_bytes == -1) {
        /* @todo */
        return PAGECACHE_UNDEF;
    }

    assert((size_t)written_bytes == PAGESZ);

    cur_idx = ilist2_put_back(page_idx_stack, *idx);
    assert(cur_idx != ILIST2_UNDEF);

    return cur_idx;
}

