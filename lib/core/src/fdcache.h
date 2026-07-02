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
 * @struct fdcache_iterator
 * @brief Structure for iterating over fdcache
 *
 * @see fdcache_begin, fdcache_next, fdcache_is_valid
 */
typedef icache_iterator fdcache_iterator;

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
 * @param cache Cache handle
 * @param hash_fn Hash function (optional)
 */
void fdcache_clear(FdCache *cache, ihash_hash_fn hash_fn);

/**
 * @brief Free cache (closes all open files)
 * @param cache Cache handle
 */
void fdcache_free(FdCache *cache);

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
 * @def fdcache_begin
 * @brief Returns the first valid iterator
 *
 * @param h         Pointer to fdcache
 * @return          Iterator to the first node
 *
 * @code
 * struct MyEntry {
 *     ssize_t key;
 *     int value;
 * };
 * FdCache *cache = fdcache_create(cache, 16, 32), *tmp;
 *
 * for (fdcache_iterator i = fdcache_begin(cache); fdcache_is_valid(cache, i); i = fdcache_next(cache, i)) {
 *     tmp = i.datum;
 *     printf("key: %li, value: %i\n", tmp->key, tmp->value);
 * }
 * @endcode
 */
#define fdcache_begin(h) icache_begin(h)

/**
 * @def fdcache_next
 * @brief Returns the next valid iterator
 *
 * @param h         Pointer to fdcache
 * @i               Previously initialized iterator
 * @return          Iterator to the next node
 *
 * @see fdcache_begin
 */
#define fdcache_next(h, i) icache_next(h, (i))

/**
 * @def fdcache_is_valid
 * @brief Returns true if iterator is valid
 *
 * @param h         Pointer to fdcache
 * @i               Previously initialized iterator
 *
 * @see fdcache_begin
 */
#define fdcache_is_valid(h, i) icache_is_valid(h, (i))

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

