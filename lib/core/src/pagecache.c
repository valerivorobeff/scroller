#include "pagecache.h"
#include "fdcache.h"
#include <unistd.h>
#include <malloc.h>
#include <assert.h>

Page g_page = NULL;
FdCache *g_fdcache = NULL;

PageCache *pagecache_create_fn(size_t bucketsz, size_t chainsz, ihash_hash_fn hash_fn);
PageCache *pagecache_init_fn(void *p, size_t bucketsz, size_t chainsz, ihash_hash_fn hash_fn);
void pagecache_clear(void *p, ihash_hash_fn hash_fn);
void *pagecache_touch_fn(icache *cache, ssize_t key);

static pagecache_idx_t pagecache_read(icache *pc, ssize_t key);
static pagecache_idx_t pagecache_write(icache *pc, ssize_t key, pagecache_idx_t idx);

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
    icache_idx_t *list = icache_get_list(cache);
    void *e = icache_get(cache, key);

    if (e != NULL)
        ilist2_move_front_by_idx(list, *(icache_idx_t *)(e + idxoffs));
    else {
        pagecache_idx_t new_page_idx = pagecache_read(cache, key);

        assert(new_page_idx != PAGECACHE_UNDEF);

        e = ihash_touch_fn(hash, key);

        if (e != NULL) {
            ((PageCache *)(e))->page_idx = new_page_idx;

            *(icache_idx_t *)(e + idxoffs) = ilist2_put_front(list, key);
            assert(*(icache_idx_t *)(e + idxoffs) != ILIST2_UNDEF);
        } else {
            ilist2_idx_t lru_key = ilist2_pop_back(list);
            size_t *old_idx;

            assert(lru_key != ILIST2_UNDEF);

            old_idx = ihash_get_member_ptr((PageCache *)hash, lru_key, page_idx);

            assert(old_idx != NULL);

            ihash_erase(hash, lru_key);

            e = ihash_touch_fn(hash, key);
            assert(e);

            pagecache_write(cache, key, *old_idx);
            ((PageCache *)(e))->page_idx = new_page_idx;

            *(icache_idx_t *)(e + idxoffs) = ilist2_put_front(list, key);
            assert(*(icache_idx_t *)(e + idxoffs) != ILIST2_UNDEF);
        }
    }

    return e;
}

pagecache_idx_t
pagecache_read(icache *pc, ssize_t key) {
    pagecache_idx_t *page_idx_stack = icache_get_extra(pc);
    pagecache_idx_t cur_idx = ilist2_pop_back(page_idx_stack);
/*    FdCache *e = fdcache_touch_fn((icache *)pcextra->fdcache, gid.full);
    ssize_t read_bytes;

    assert(cur_idx != ILIST2_UNDEF);
    assert(e != NULL);

    read_bytes = read(e->fd, (Page)icache_get_extra(pcextra->fdcache), 8096);

    assert(read_bytes == 8096);
*/
    return cur_idx;
}

pagecache_idx_t
pagecache_write(icache *pc, ssize_t key, pagecache_idx_t idx) {
    pagecache_idx_t *page_idx_stack = icache_get_extra(pc);
    pagecache_idx_t cur_idx = ilist2_pop_back(page_idx_stack);
    /*pagecache_idx_t *pcextra = icache_get_extra(pc);
    pagecache_idx_t cur_idx = ilist2_put_back(pcextra->page_idx_stack, idx);
    FdCache *e = fdcache_touch_fn((icache *)pcextra->fdcache, gid.full);
    ssize_t written_bytes;

    assert(cur_idx != ILIST2_UNDEF);
    assert(e != NULL);

    written_bytes = write(e->fd, (Page)icache_get_extra(pcextra->fdcache), 8096);

    assert(written_bytes == 8096);
*/
    return cur_idx;
}

