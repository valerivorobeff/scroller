/**
 * @file fcache.h
 * @brief File descriptor cache
 *
 */

#ifndef _FCACHE_H_
#define _FCACHE_H_

#include "gid.h"
#include "grid.h"
#include "icache.h"

typedef struct fcache {
    Gid key;
    int fd;
} fcache;

#define fcache_create(h, bucketsz_, chainsz_, hash_fn_) \
    icache_create(h, bucketsz_, chainsz_, hash_fn_)

#define fcache_free(h) \
    icache_free(h)

void *fcache_touch_fn(icache *cache, ssize_t key);

#endif /* _FCACHE_H_ */

