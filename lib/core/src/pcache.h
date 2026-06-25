/**
 * @file pcache.h
 * @brief Page cache
 *
 */

#ifndef _PCACHE_H_
#define _PCACHE_H_

#include "gid.h"
#include "grid.h"
#include "icache.h"

/**
 * @def PCACHE_UNDEF
 * @brief Sentinel value representing "no entry"
 *
 * This value (-1) is used as:
 * - Empty slot marker in cache entries
 * - Invalid index for node allocation
 *
 * @warning Indices with value -1 are not supported
 */
#define PCACHE_UNDEF (ssize_t)-1

typedef struct fcache fcache;

typedef ssize_t pcache_idx_t;

typedef struct pcache {
    Gid key;
    size_t page_idx;
} pcache;

#define pcache_create(h, bucketsz_, chainsz_, hash_fn_, pcache_) \
    icache_create(h, bucketsz_, chainsz_, hash_fn_, pcache_, 0)

#define pcache_free(h) \
    icache_free(h)

pcache *pcache_init_fn(void *p, size_t bucketsz, size_t chainsz, size_t keyoffs, size_t usersz, ihash_hash_fn hash_fn, fcache *fcache);
pcache *pcache_touch_fn(pcache *pc, Gid gid);

#endif /* _PCACHE_H_ */

