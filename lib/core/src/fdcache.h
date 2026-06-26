/**
 * @file fdCache.h
 * @brief File descriptor cache
 *
 */

#ifndef _FDCACHE_H_
#define _FDCACHE_H_

#include "gid.h"
#include "grid.h"
#include "icache.h"

typedef struct FdCache {
    Gid key;
    int fd;
} FdCache;

#define fdcache_create(h, bucketsz_, chainsz_, hash_fn_) \
    icache_create(h, bucketsz_, chainsz_, hash_fn_, sizeof(Page))

#define fdcache_free(h) \
    icache_free(h)

FdCache *fdcache_touch_fn(FdCache *fc, Gid gid);

#endif /* _FDCACHE_H_ */

