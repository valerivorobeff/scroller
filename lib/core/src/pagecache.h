/**
 * @file pagecache.h
 * @brief Page cache
 *
 */

#ifndef _PAGECACHE_H_
#define _PAGECACHE_H_

#include "gid.h"
#include "grid.h"
#include "icache.h"

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

#define pagecache_create(h, bucketsz_, chainsz_, hash_fn_, pcache_) \
    icache_create(h, bucketsz_, chainsz_, hash_fn_, pcache_, 0)

#define pagecache_free(h) \
    icache_free(h)

#define pagecache_get_required_memory_size(bucketsz, chainsz, usersz, extrasz) \
        icache_get_required_memory_size(bucketsz, chainsz, usersz + sizeof(icache_idx_t)) + \
        ilist2_get_required_memory_size((bucketsz) + (chainsz), sizeof(icache_idx_t)) + \
        extrasz \
    )

PageCache *pagecache_init_fn(void *p, size_t bucketsz, size_t chainsz, size_t keyoffs, size_t usersz, ihash_hash_fn hash_fn, FdCache *fdcache);
PageCache *pagecache_touch_fn(PageCache *pc, Gid gid);

#endif /* _PAGECACHE_H_ */

