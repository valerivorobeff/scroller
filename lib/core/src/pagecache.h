/**
 * @file pagecache.h
 * @brief Page cache
 *
 */

#ifndef _PAGECACHE_H_
#define _PAGECACHE_H_

#include "gid.h"
#include "grid.h"
#include "fdcache.h"

extern Page g_pages;
extern FdCache *g_fdcache;

/**
 * @def PAGECACHE_UNDEF
 * @brief Sentinel value representing "no entry"
 *
 * This value (-1) is used as:
 * - Empty slot marker in cache entries
 * - Invalid index for node allocation
 *
 * @warning Indices with value -1 are not supported
 */
#define PAGECACHE_UNDEF (ssize_t)-1

typedef struct FdCache FdCache;

typedef ssize_t pagecache_idx_t;

typedef struct PageCache {
    Gid key;
    size_t page_idx;
} PageCache;

#define pagecache_create(bucketsz_, chainsz_, hash_fn_) \
    pagecache_create_fn(bucketsz_, chainsz_, hash_fn_)

#define pagecache_init(h, bucketsz_, chainsz_, hash_fn_) \
    pagecache_init_fn(h, bucketsz_, chainsz_, hash_fn_)

#define pagecache_free(h) \
    icache_free(h)

#define pagecache_put(h, key_) \
    (PageCache *)pagecache_touch_fn((icache *)h, key_)

#define pagecache_put_page(h, key_) \
    ({ \
        PageCache *_e_ = pagecache_put(h, key_); \
        _e_ ? (g_pages + _e_->page_idx * PAGESZ) : NULL; \
    })

#define pagecache_get_required_memory_size(bucketsz, chainsz) \
        icache_get_required_memory_size(bucketsz, chainsz, sizeof(Page), \
        pagecache_get_extra_size(bucketsz + chainsz))

#define pagecache_get_extra_size(pagenum) \
        ilist2_get_required_memory_size(pagenum, sizeof(pagecache_idx_t))

PageCache *pagecache_create_fn(size_t bucketsz, size_t chainsz, ihash_hash_fn hash_fn);
PageCache *pagecache_init_fn(void *p, size_t bucketsz, size_t chainsz, ihash_hash_fn hash_fn);
void pagecache_clear(void *p, ihash_hash_fn hash_fn);
void *pagecache_touch_fn(icache *cache, ssize_t key);
ssize_t pagecache_flush(PageCache *cache, ssize_t key);

#endif /* _PAGECACHE_H_ */

