/**
 * @file icache.c
 * @brief Implementation of index-based cache
 *
 * This file implements an lru cache that uses indices instead of pointers
 * for node linking, making it suitable for shared memory and persistent
 * storage scenarios. It uses ihash hash table to store the cache data and
 * ilist2 list to sort least recently used (LRU) nodes.
 *
 *
 * Memory Layout:
 * ```
 * +-------------+
 * | icache      | Header (hashoffs, listoffd, nodesz)
 * +-------------+
 * | ihash       | Hash table
 * +-------------+
 * | ilist2      | LRU list
 * +-------------+
 * | Extra data  | Extra data
 * +-------------+
 * ```
 *
 * @see icache.h
 */

#include "icache.h"
#include <malloc.h>
#include <assert.h>

/**
 * @cond PRIVATE
 * Public functions - forward declarations
 * @endcond
 */

icache *icache_create_fn(size_t bucketsz, size_t chainsz, size_t keyoffs, size_t usersz, ihash_hash_fn hash_fn, size_t extrasz);
icache *icache_init_fn(void *p, size_t bucketsz, size_t chainsz, size_t keyoffs, size_t usersz, ihash_hash_fn hash_fn, size_t extrasz);
void icache_clear(void *p, ihash_hash_fn hash_fn);
void icache_free(void *p);
void *icache_touch_fn(icache *cache, ssize_t key);

/**
 * @brief Creates and initializes a new cache
 *
 * Memory allocation:
 * - Header: sizeof(icache) bytes
 * - Hash table
 * - LRU list
 *
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
 * @note The cache is relocatable - all references are indices, not pointers
 */
icache *
icache_create_fn(size_t bucketsz, size_t chainsz, size_t keyoffs, size_t usersz, ihash_hash_fn hash_fn, size_t extrasz) {
    icache *cache;

    if (bucketsz == 0)
        return NULL;    /* Error, impossible to init a hash without buckets */

    /* Allocate contiguous memory */
    cache = malloc(icache_get_required_memory_size(bucketsz, chainsz, usersz, extrasz));

    return cache ? icache_init_fn(cache, bucketsz, chainsz, keyoffs, usersz, hash_fn, extrasz) : NULL;
}

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
icache *
icache_init_fn(void *p, size_t bucketsz, size_t chainsz, size_t keyoffs, size_t usersz, ihash_hash_fn hash_fn, size_t extrasz) {
    icache *cache = p;
    const size_t nodesz = usersz + sizeof(icache_idx_t);

    if (bucketsz == 0)
        return NULL;    /* Error, impossible to init a hash without buckets */

    cache->hashoffs = sizeof(icache);
    cache->listoffs = cache->hashoffs + ihash_get_required_memory_size(bucketsz, chainsz, nodesz);
    cache->extraoffs = cache->listoffs + ilist2_get_required_memory_size(bucketsz + chainsz, sizeof(icache_idx_t));
    cache->nodesz = nodesz;
    cache->extrasz = extrasz;

    if (ihash_init_fn(icache_get_hash(cache), bucketsz, chainsz, keyoffs, nodesz, hash_fn) == NULL)
        return NULL;

    if (ilist2_init_fn(icache_get_list(cache), bucketsz + chainsz, sizeof(icache_idx_t)) == NULL)
        return NULL;

    return cache;
}

/**
 * @brief Clears all entries from the cache
 *
 * Resets the cache to empty state, as if it was just initialized.
 *
 * @param p         Pointer to cache to clear
 * @param hash_fn   Pointer to user defined hash function of type ihash_hash_fn or
 *                  NULL to use default function
 *
 * @pre cache must be a valid, initialized cache
 *
 * @note Time complexity: O(bucketsz + chainsz)
 * @warning Does NOT call any destructor on stored values
 * @warning All previously returned pointers become invalid
 *
 * @code
 * struct MyEntry *cache = icache_create(cache, 16, 32);
 * icache_put(cache, 42, 100);
 * icache_put(cache, 43, 200);
 *
 * icache_clear(cache);  // cache becomes empty
 *
 * assert(icache_get(hash, 42) == NULL);
 * @endcode
 */
void
icache_clear(void *p, ihash_hash_fn hash_fn) {
    ihash_clear(icache_get_hash(p), hash_fn);
    ilist2_clear(icache_get_list(p));
}

/**
 * @brief Frees the cache memory
 * @param cache Pointer to hash table to free
 *
 * @note Just a wrapper around free() for API consistency
 */
void
icache_free(void *p) {
    free(p);
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
void *
icache_touch_fn(icache *cache, ssize_t key) {
    const icache_idx_t idxoffs = cache->nodesz - sizeof(icache_idx_t);
    ihash *hash = icache_get_hash(cache);
    icache_idx_t *list = icache_get_list(cache);
    void *e = icache_get(cache, key);

    if (e != NULL)
        ilist2_move_front_by_idx(list, *(icache_idx_t *)(e + idxoffs));
    else {
        e = ihash_touch_fn(hash, key);

        if (e != NULL) {
            *(icache_idx_t *)(e + idxoffs) = ilist2_put_front(list, key);
            assert(*(icache_idx_t *)(e + idxoffs) != ILIST2_UNDEF);
        } else {
            const icache_idx_t lru_key = ilist2_pop_back(list);

            assert(lru_key != ILIST2_UNDEF);
            assert(ihash_exists(hash, lru_key));

            ihash_erase(hash, lru_key);
            e = ihash_touch_fn(hash, key);
            if (e == NULL)
                return NULL;

            *(icache_idx_t *)(e + idxoffs) = ilist2_put_front(list, key);
            assert(*(icache_idx_t *)(e + idxoffs) != ILIST2_UNDEF);
        }
    }

    return e;
}

