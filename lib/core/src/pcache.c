#include "pcache.h"
#include "fcache.h"
#include <unistd.h>
#include <assert.h>

pcache *pcache_init_fn(void *p, size_t bucketsz, size_t chainsz, size_t keyoffs, size_t usersz, ihash_hash_fn hash_fn, fcache *fcache);
pcache *pcache_touch_fn(pcache *pc, Gid gid);

static pcache_idx_t pcache_read(icache *pc, Gid gid);
static pcache_idx_t pcache_write(icache *pc, Gid gid, pcache_idx_t idx);

typedef struct pcache_extra {
    fcache *fcache;
    pcache_idx_t *page_idx_stack;
} pcache_extra;

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
pcache *
pcache_init_fn(void *p, size_t bucketsz, size_t chainsz, size_t keyoffs, size_t usersz, ihash_hash_fn hash_fn, fcache *fcache_) {
    const size_t fullsz = bucketsz + chainsz;
    icache *cache = icache_init_fn(p, bucketsz, chainsz, keyoffs, usersz, hash_fn, 
            sizeof(struct fcache *) + ilist2_get_required_memory_size(fullsz, sizeof(pcache_idx_t)));
    pcache_extra *pcache_extra;

    if (cache == NULL)
        return NULL;

    pcache_extra = icache_get_extra(cache);
    pcache_extra->fcache = fcache_;
    pcache_extra->page_idx_stack = (pcache_idx_t *)(pcache_extra->fcache + 1);

    pcache_extra->page_idx_stack = ilist2_init(pcache_extra->page_idx_stack, fullsz);

    if (pcache_extra->page_idx_stack == NULL)
        return NULL;

    for (pcache_idx_t i = fullsz - 1; i >= 0; --i)
        ilist2_put_back(pcache_extra->page_idx_stack, i);

    return (pcache *)cache;
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
pcache *
pcache_touch_fn(pcache *pc, Gid gid) {
    icache *cache = (icache *)pc;
    const icache_idx_t idxoffs = cache->nodesz - sizeof(icache_idx_t);
    ihash *hash = icache_get_hash(cache);
    icache_idx_t *list = icache_get_list(cache);
    pcache *e = (pcache *)icache_get(cache, gid.full);

    if (e != NULL)
        ilist2_move_front_by_idx(list, *(icache_idx_t *)(e + idxoffs));
    else {
        pcache_idx_t new_page_idx = pcache_read(cache, gid);

        assert(new_page_idx != PCACHE_UNDEF);

        e = ihash_touch_fn(hash, gid.full);

        if (e != NULL) {
            e->page_idx = new_page_idx;

            *(icache_idx_t *)(e + idxoffs) = ilist2_put_front(list, gid.full);
            assert(*(icache_idx_t *)(e + idxoffs) != ILIST2_UNDEF);
        } else {
            ilist2_idx_t lru_key = ilist2_pop_back(list);
            size_t *old_idx;

            assert(lru_key != ILIST2_UNDEF);

            old_idx = ihash_get_member_ptr((pcache *)hash, lru_key, page_idx);

            assert(old_idx != NULL);

            ihash_erase(hash, lru_key);

            e = ihash_touch_fn(hash, gid.full);
            assert(e);

            pcache_write(cache, gid, *old_idx);
            e->page_idx = new_page_idx;

            *(icache_idx_t *)(e + idxoffs) = ilist2_put_front(list, gid.full);
            assert(*(icache_idx_t *)(e + idxoffs) != ILIST2_UNDEF);
        }
    }

    return e;
}

pcache_idx_t
pcache_read(icache *pc, Gid gid) {
    pcache_extra *pcache_extra = icache_get_extra(pc);
    pcache_idx_t cur_idx = ilist2_pop_back(pcache_extra->page_idx_stack);
    fcache *e = fcache_touch_fn(pcache_extra->fcache, gid);
    ssize_t read_bytes;

    assert(cur_idx != ILIST2_UNDEF);
    assert(e != NULL);

    read_bytes = read(e->fd, (Page)icache_get_extra(pcache_extra->fcache), 8096);

    assert(read_bytes == 8096);

    return cur_idx;
}

pcache_idx_t
pcache_write(icache *pc, Gid gid, pcache_idx_t idx) {
    pcache_extra *pcache_extra = icache_get_extra(pc);
    pcache_idx_t cur_idx = ilist2_put_back(pcache_extra->page_idx_stack, idx);
    fcache *e = fcache_touch_fn(pcache_extra->fcache, gid);
    ssize_t written_bytes;

    assert(cur_idx != ILIST2_UNDEF);
    assert(e != NULL);

    written_bytes = write(e->fd, (Page)icache_get_extra(pcache_extra->fcache), 8096);

    assert(written_bytes == 8096);

    return cur_idx;
}

