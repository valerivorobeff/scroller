/**
 * @file fdcache.h
 * @brief File descriptor cache
 *
 */

#ifndef _FDCACHE_H_
#define _FDCACHE_H_

#include "gid.h"
#include "grid.h"
#include "icache.h"

typedef struct FdCache {
    ssize_t key;
    int fd;
} FdCache;

#define fdcache_create(h, bucketsz_, chainsz_, hash_fn_) \
    icache_create(h, bucketsz_, chainsz_, hash_fn_, 0)

#define fdcache_init(h, bucketsz_, chainsz_, hash_fn_) \
    icache_init(h, bucketsz_, chainsz_, hash_fn_, 0)

#define fdcache_clear(h, hash_fn) \
    icache_clear(h, hash_fn)

#define fdcache_free(h) \
    icache_free(h)

#define fdcache_get(h, key_) \
    icache_get(h, key_)

#define fdcache_exists(h, key_) \
    icache_exists(h, key_)

#define fdcache_get_fd_ptr(h, key_) \
    icache_get_member_ptr(h, key_, fd)

#define fdcache_get_fd(h, key_) \
    icache_get_member(h, (ssize_t)key_, fd)

#define fdcache_put(h, key_) \
    (typeof(h))fdcache_touch_fn((icache *)h, key_);

#define fdcache_get_required_memory_size(bucketsz, chainsz) \
    icache_get_required_memory_size(bucketsz, chainsz, sizeof(FdCache), 0)

void *fdcache_touch_fn(icache *cache, ssize_t key);

#endif /* _FDCACHE_H_ */

