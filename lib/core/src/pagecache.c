#include "pagecache.h"
#include "fdcache.h"
#include <unistd.h>
#include <assert.h>

PageCache *pagecache_init_fn(void *p, size_t bucketsz, size_t chainsz, size_t keyoffs, size_t usersz, ihash_hash_fn hash_fn, FdCache *fdcache);
PageCache *pagecache_touch_fn(PageCache *pc, Gid gid);

static pagecache_idx_t pagecache_read(icache *pc, Gid gid);
static pagecache_idx_t pagecache_write(icache *pc, Gid gid, pagecache_idx_t idx);

typedef struct PageCacheExtra {
    FdCache *fdcache;
    pagecache_idx_t *page_idx_stack;
} PageCacheExtra;

/*
PageCache *
pagecache_create_fn(size_t bucketsz, size_t chainsz, size_t keyoffs, size_t usersz, ihash_hash_fn hash_fn, FdCache *fdcache_) {

}
*/
/**
 * @brief Initializes a cache in pre-allocated memory
 *
 * Memory usage:
 * - Header: sizeof(icache) bytes
 * - Hash table
 * - LRU list
 *
 * @param p        Previously allocated pointer where icache is going to be initialized
 * @param bucketsz Number of primary hash slots (must be > 0)
 * @param chainsz  Size of overflow node pool (can be 0)
 * @param keyoffs  Byte offset of 'key' field within entry
 * @param usersz   Total size of each user's entry in bytes
 * @param hash_fn  Pointer to user defined hash function of type ihash_hash_fn or
 *                 NULL to use default function
 * @param extrasz  Extra data size
 * @return         Pointer to initialized cache, or NULL on allocation failure
 *
 * @pre bucketsz > 0
 * @pre usersz + sizeof(icache_idx_t) >= keyoffs + sizeof(ssize_t)
 *
 * @note The cache is relocatable - all references are indices, not pointers.
 * @note It does not allocate memory, the caller must provide a pre-allocated buffer.
 * @note Use icache_get_required_memory_size macro to know number of bytes required for cache.
 */
PageCache *
pagecache_init_fn(void *p, size_t bucketsz, size_t chainsz, size_t keyoffs, size_t usersz, ihash_hash_fn hash_fn, FdCache *fdcache_) {
    const size_t fullsz = bucketsz + chainsz;
    icache *cache = icache_init_fn(p, bucketsz, chainsz, keyoffs, usersz, hash_fn, 
            sizeof(struct FdCache *) + ilist2_get_required_memory_size(fullsz, sizeof(pagecache_idx_t)));
    PageCacheExtra *pcextra;

    if (cache == NULL)
        return NULL;

    pcextra = icache_get_extra(cache);
    pcextra->fdcache = fdcache_;
    pcextra->page_idx_stack = (pagecache_idx_t *)(pcextra->fdcache + 1);

    pcextra->page_idx_stack = ilist2_init(pcextra->page_idx_stack, fullsz);

    if (pcextra->page_idx_stack == NULL)
        return NULL;

    for (pagecache_idx_t i = fullsz - 1; i >= 0; --i)
        ilist2_put_back(pcextra->page_idx_stack, i);

    return (PageCache *)cache;
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
PageCache *
pagecache_touch_fn(PageCache *pc, Gid gid) {
    icache *cache = (icache *)pc;
    const icache_idx_t idxoffs = cache->nodesz - sizeof(icache_idx_t);
    ihash *hash = icache_get_hash(cache);
    icache_idx_t *list = icache_get_list(cache);
    PageCache *e = (PageCache *)icache_get(cache, gid.full);

    if (e != NULL)
        ilist2_move_front_by_idx(list, *(icache_idx_t *)(e + idxoffs));
    else {
        pagecache_idx_t new_page_idx = pagecache_read(cache, gid);

        assert(new_page_idx != PAGECACHE_UNDEF);

        e = ihash_touch_fn(hash, gid.full);

        if (e != NULL) {
            e->page_idx = new_page_idx;

            *(icache_idx_t *)(e + idxoffs) = ilist2_put_front(list, gid.full);
            assert(*(icache_idx_t *)(e + idxoffs) != ILIST2_UNDEF);
        } else {
            ilist2_idx_t lru_key = ilist2_pop_back(list);
            size_t *old_idx;

            assert(lru_key != ILIST2_UNDEF);

            old_idx = ihash_get_member_ptr((PageCache *)hash, lru_key, page_idx);

            assert(old_idx != NULL);

            ihash_erase(hash, lru_key);

            e = ihash_touch_fn(hash, gid.full);
            assert(e);

            pagecache_write(cache, gid, *old_idx);
            e->page_idx = new_page_idx;

            *(icache_idx_t *)(e + idxoffs) = ilist2_put_front(list, gid.full);
            assert(*(icache_idx_t *)(e + idxoffs) != ILIST2_UNDEF);
        }
    }

    return e;
}

pagecache_idx_t
pagecache_read(icache *pc, Gid gid) {
    PageCacheExtra *pcextra = icache_get_extra(pc);
    pagecache_idx_t cur_idx = ilist2_pop_back(pcextra->page_idx_stack);
    FdCache *e = fdcache_touch_fn(pcextra->fdcache, gid);
    ssize_t read_bytes;

    assert(cur_idx != ILIST2_UNDEF);
    assert(e != NULL);

    read_bytes = read(e->fd, (Page)icache_get_extra(pcextra->fdcache), 8096);

    assert(read_bytes == 8096);

    return cur_idx;
}

pagecache_idx_t
pagecache_write(icache *pc, Gid gid, pagecache_idx_t idx) {
    PageCacheExtra *pcextra = icache_get_extra(pc);
    pagecache_idx_t cur_idx = ilist2_put_back(pcextra->page_idx_stack, idx);
    FdCache *e = fdcache_touch_fn(pcextra->fdcache, gid);
    ssize_t written_bytes;

    assert(cur_idx != ILIST2_UNDEF);
    assert(e != NULL);

    written_bytes = write(e->fd, (Page)icache_get_extra(pcextra->fdcache), 8096);

    assert(written_bytes == 8096);

    return cur_idx;
}

