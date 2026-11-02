
/**
 * @file pagecache.c
 * @brief Implementation of page cache
 *
 * This file implements an lru page cache that uses icache as the base cache
 * functional. It also user icache extra block to store mutex for icache
 * lru ilist2 and for page index cache which is aimed at allocating freepage
 * indices.
 *
 *
 * Memory Layout:
 * ```
 * +-------------+
 * | icache      | Header, ihash, ilist2
 * +-------------+
 * | mutex       | Mutex for ilist2 (placed in icache extra data)
 * +-------------+
 * | page indices| Page indices (placed in icache extra data)
 * +-------------+
 * ```
 *
 * @see icache.h
 */

#include "pagecache.h"
#include <unistd.h>
#include <malloc.h>
#include <assert.h>

Page g_pages = NULL;
FdCache *g_fdcache = NULL;

PageCache *pagecache_create_fn(size_t bucketsz, size_t chainsz, ihash_hash_fn hash_fn);
PageCache *pagecache_init_fn(void *p, size_t bucketsz, size_t chainsz, ihash_hash_fn hash_fn);
void pagecache_clear(void *p, ihash_hash_fn hash_fn);
void *pagecache_touch_fn(icache *cache, Gid key);
ssize_t pagecache_flush(PageCache *cache, Gid key);

static pagecache_idx_t pagecache_read(icache *cache, Gid key);
static pagecache_idx_t pagecache_write(icache *cache, Gid key);
static int mutex_init(pthread_mutex_t *mtx);

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
    icache *cache = (icache *)icache_init((PageCache *)p, bucketsz, chainsz, hash_fn,
        pagecache_get_extra_size(fullsz));
    PageCacheExtra *extra;
    pagecache_idx_t *page_idx_stack;

    /* We need to be sure key sizes of pagecache and icache are equal */
    assert(sizeof(Gid) == sizeof(ssize_t));

    if (cache == NULL)
        return NULL;

    extra = icache_get_extra(cache);

    /* Init lru ilist2 mutex */
    if (mutex_init(&extra->lru_mtx))
        return NULL;

    page_idx_stack = extra->page_idx_stack;
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
pagecache_touch_fn(icache *cache, Gid key) {
    const icache_idx_t idxoffs = cache->nodesz - sizeof(icache_idx_t);
    ihash *hash = icache_get_hash(cache);
    icache_idx_t *lru_list = icache_get_list(cache);
    PageCacheExtra *extra = icache_get_extra(cache);

    pthread_mutex_lock(&extra->lru_mtx);
    void *e = icache_get(cache, key.full);

    if (e != NULL) {
                                                            /* Make node mru */
        ilist2_move_front_by_idx(lru_list, *(icache_idx_t *)(e + idxoffs));
    } else {
        e = ihash_touch_fn(hash, key.full);                 /* Put new key in hash */

        if (e == NULL) {                                    /* Hash is full, erase lru node */
            ilist2_idx_t lru_key = ilist2_pop_back(lru_list); /* Pop lru key from lru list */
            assert(lru_key != ILIST2_UNDEF);

            pagecache_write(cache, (Gid){ .full = lru_key });   /* Save lru page to disk */
            ihash_erase(hash, lru_key);                     /* Erase it from hash */

            e = ihash_touch_fn(hash, key.full);                  /* Put new node to hash */
            assert(e);

            /* Init PageCache mutex */
            if (mutex_init(&((PageCache *)e)->mtx)) {
                ihash_erase(hash, key.full);
                pthread_mutex_unlock(&extra->lru_mtx);

                return NULL;
            }
        }

                                                            /* Put lru hash node's idx into list */
        *(icache_idx_t *)(e + idxoffs) = ilist2_put_front(lru_list, key.full);
        assert(*(icache_idx_t *)(e + idxoffs) != ILIST2_UNDEF);

        pthread_mutex_lock(&((PageCache *)e)->mtx);

        ((PageCache *)(e))->page_idx = pagecache_read(cache, key);
        assert((pagecache_idx_t)((PageCache *)(e))->page_idx != PAGECACHE_UNDEF);

        pthread_mutex_unlock(&((PageCache *)e)->mtx);
    }

    pthread_mutex_unlock(&extra->lru_mtx);

    return e;
}

ssize_t
pagecache_flush(PageCache *cache, Gid key) {
    const FdCache *e = fdcache_put(g_fdcache, key.parts.file_id);
    const size_t *idx = icache_get_member_ptr(cache, key.full, page_idx);
    ssize_t written_bytes;

    assert(e != NULL);
    assert(idx != NULL);

    written_bytes = pwrite(e->fd, g_pages + *idx * PAGESZ, PAGESZ, key.parts.page * PAGESZ);

    return written_bytes != (ssize_t)PAGESZ ? written_bytes : 0;
}

static pagecache_idx_t
pagecache_read(icache *cache, Gid key) {
    const PageCacheExtra *extra = icache_get_extra(cache);
    const pagecache_idx_t *page_idx_stack = extra->page_idx_stack;
    const pagecache_idx_t cur_idx = ilist2_pop_back(page_idx_stack);
    const FdCache *e = fdcache_put(g_fdcache, key.parts.file_id);
    ssize_t read_bytes;

    assert(cur_idx != ILIST2_UNDEF);
    assert(e != NULL);

    read_bytes = pread(e->fd, g_pages + cur_idx * PAGESZ, PAGESZ, key.parts.page * PAGESZ);
    if (read_bytes == -1) {
        return PAGECACHE_UNDEF;
    }

    /* We shouldn't check read_bytes == PAGESZ here because
     * the file can be new and empty */

    return cur_idx;
}

static pagecache_idx_t
pagecache_write(icache *cache, Gid key) {
    PageCacheExtra *extra = icache_get_extra(cache);
    pagecache_idx_t *page_idx_stack = extra->page_idx_stack;
    pagecache_idx_t cur_idx;
    const FdCache *e = fdcache_put(g_fdcache, key.parts.file_id);
    const size_t *idx = icache_get_member_ptr((PageCache *)cache, key.full, page_idx);
    ssize_t written_bytes;

    assert(e != NULL);
    assert(idx != NULL);

    written_bytes = pwrite(e->fd, g_pages + *idx * PAGESZ, PAGESZ, key.parts.page * PAGESZ);
    if (written_bytes == -1) {
        return PAGECACHE_UNDEF;
    }

    assert((size_t)written_bytes == PAGESZ);

    cur_idx = ilist2_put_back(page_idx_stack, *idx);
    assert(cur_idx != ILIST2_UNDEF);

    return cur_idx;
}

static int
mutex_init(pthread_mutex_t *mtx) {
    int ret;
    pthread_mutexattr_t ma;

    ret = pthread_mutexattr_init(&ma);
        return ret;

    pthread_mutexattr_setpshared(&ma, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_setrobust(&ma, PTHREAD_MUTEX_ROBUST);

    ret = pthread_mutex_init(mtx, &ma);

    pthread_mutexattr_destroy(&ma);

    return ret;
}

