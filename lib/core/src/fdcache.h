/**
 * @file fdcache.h
 * @brief File descriptor cache
 *
 * LRU cache for file descriptors. Wraps icache with automatic
 * file open/close on cache insertion/eviction.
 */

#ifndef _FDCACHE_H_
#define _FDCACHE_H_

#include "gid.h"
#include "grid.h"
#include "icache.h"

/**
 * @brief Cache entry structure
 */
typedef struct FdCache {
    ssize_t key;    /** Cache key (Gid.full) */
    int fd;         /** Open file descriptor */
} FdCache;

/**
 * @brief Create new fdcache
 * @param h Buffer for cache (or NULL to allocate)
 * @param bucketsz_ Number of buckets
 * @param chainsz_ Number of chain entries
 * @param hash_fn_ Hash function (optional)
 * @return Pointer to cache, or NULL on error
 */
#define fdcache_create(h, bucketsz_, chainsz_, hash_fn_) \
    icache_create(h, bucketsz_, chainsz_, hash_fn_, 0)

/**
 * @brief Initialize fdcache in existing buffer
 * @param h Buffer for cache
 * @param bucketsz_ Number of buckets
 * @param chainsz_ Number of chain entries
 * @param hash_fn_ Hash function (optional)
 * @return Pointer to cache
 */
#define fdcache_init(h, bucketsz_, chainsz_, hash_fn_) \
    icache_init(h, bucketsz_, chainsz_, hash_fn_, 0)

/**
 * @brief Clear cache (closes all open files)
 * @param h Cache handle
 * @param hash_fn Hash function (optional)
 */
#define fdcache_clear(h, hash_fn) \
    icache_clear(h, hash_fn)

/**
 * @brief Free cache (closes all open files)
 * @param h Cache handle
 */
#define fdcache_free(h) \
    icache_free(h)

/**
 * @brief Get cached entry by key
 * @param h Cache handle
 * @param key_ Key to lookup
 * @return Pointer to FdCache entry, or NULL
 */
#define fdcache_get(h, key_) \
    icache_get(h, key_)

/**
 * @brief Check if key exists in cache
 * @param h Cache handle
 * @param key_ Key to check
 * @return 1 if exists, 0 otherwise
 */
#define fdcache_exists(h, key_) \
    icache_exists(h, key_)

/**
 * @brief Get pointer to fd member
 * @param h Cache handle
 * @param key_ Key to lookup
 * @return Pointer to fd, or NULL
 */
#define fdcache_get_fd_ptr(h, key_) \
    icache_get_member_ptr(h, key_, fd)

/**
 * @brief Get fd value
 * @param h Cache handle
 * @param key_ Key to lookup
 * @return fd value, or 0 if not found
 */
#define fdcache_get_fd(h, key_) \
    icache_get_member(h, key_, fd)

/**
 * @brief Put/touch entry in cache (opens file if needed)
 * @param h Cache handle
 * @param key_ Key to put
 * @return Pointer to FdCache entry, or NULL on error
 */
#define fdcache_put(h, key_) \
    (typeof(h))fdcache_touch_fn((icache *)h, key_);

/**
 * @brief Get required memory size for cache
 * @param bucketsz_ Number of buckets
 * @param chainsz_ Number of chain entries
 * @return Size in bytes
 */
#define fdcache_get_required_memory_size(bucketsz, chainsz) \
    icache_get_required_memory_size(bucketsz, chainsz, sizeof(FdCache), 0)

/**
 * @brief Touch or create cache entry (internal)
 * @param cache Cache handle
 * @param key Key to touch
 * @return Pointer to cache entry
 */
void *fdcache_touch_fn(icache *cache, ssize_t key);

#endif /* _FDCACHE_H_ */

