/**
 * @file icache.c
 * @brief Implementation of index-based hash table
 *
 * This file implements a hash table that uses indices instead of pointers
 * for node linking, making it suitable for shared memory and persistent
 * storage scenarios.
 *
 * @section algorithm Hash Table Algorithm
 *
 * The hash table uses separate chaining with:
 * 1. A fixed-size array of primary slots (bucketsz)
 * 2. A pool of overflow nodes (chainsz)
 * 3. A freelist for node management
 *
 * Memory Layout:
 * ```
 * +-------------+
 * | icache       | Header (bucketsz, chainsz, chain_head)
 * +-------------+
 * | Slot 0      | Primary hash slots
 * | Slot 1      | (bucket entries)
 * | ...         |
 * | Slot N-1    |
 * +-------------+
 * | Node 0      | Node pool entries
 * | Node 1      | (chain entries)
 * | ...         |
 * | Node M-1    |
 * +-------------+
 * ```
 *
 * @see icache.h
 */

/**
 * @todo:
 * 1. correct icache_foreach()
 * 2. Think of interface.
 *      icache_get* and icache_put* macro names don't match
 *      now: icache_get -> icache_put_struct maybe: icache_get_struct, icache_put_struct
 *      icache_get_member_ptr and icache_get_member don't have their analogs of icache_put*s
 * 3. Investigate if it makes more sence to move all the entries including the first one
 *      into the chains pool and store in the buckets only the index of the first entry.
 */

#include "icache.h"
#include "ilist2.h"
#include <malloc.h>
#include <string.h>
#include <assert.h>

/**
 * @cond PRIVATE
 * Static functions and data
 * @endcond
 */

/**
 * @brief Internal helper to get pointer to hash table primary buckets
 * @param h Pointer to hash table header
 * @return Pointer to the start of primary hash buckets
 */
#define icache_get_hash(h)   ((void *)((char *)h + sizeof(icache)))

/**
 * @brief Internal helper to get pointer to chain pool
 * @param h Pointer to hash table header
 * @return Pointer to the start of chain pool
 */
#define icache_get_list(h) ((char *)icache_get_buckets(h) + ((icache *)(h))->nodesz * ((icache*)(h))->bucketsz)

/**
 * @cond PRIVATE
 * Public functions - forward declarations
 * @endcond
 */

void icache_free(void *hash);
icache *icache_create_fn(size_t bucketsz, size_t chainsz, size_t keyoffs, size_t usersz, ihash_hash_fn hash_fn);
icache *icache_init_fn(void *p, size_t bucketsz, size_t chainsz, size_t keyoffs, size_t usersz, ihash_hash_fn hash_fn);
void icache_clear(void *p, ihash_hash_fn hash_fn);
//void *icache_get_fn(icache *hash, ssize_t key);
void *icache_touch_fn(icache *cache, ssize_t key);
//int icache_erase_fn(icache *hash, ssize_t key);

/**
 * @cond PRIVATE
 * Implementation
 * @endcond
 */

/**
 * @brief Frees the hash table memory
 * @param hash Pointer to hash table to free
 *
 * @note Just a wrapper around free() for API consistency
 */
void
icache_free(void *hash) {
    free((icache *)hash);
}

/**
 * @brief Creates and initializes a new hash table
 *
 * Memory allocation:
 * - Header: sizeof(icache) bytes
 * - Primary slots: entrysz * bucketsz bytes
 * - Node pool: entrysz * chainsz bytes
 *
 * Initialization:
 * 1. All primary slot entries have key = ICACHE_UNDEF, next = ICACHE_UNDEF
 * 2. Node pool entries form a linked list via 'next' indices
 * 3. chain_head points to first free node (0)
 * 4. Last node's next = ICACHE_UNDEF
 *
 * @param bucketsz Number of primary hash slots (must be > 0)
 * @param chainsz  Size of overflow node pool (can be 0)
 * @param keyoffs  Byte offset of 'key' field within entry
 * @param usersz   Total size of each user's entry in bytes
 * @param hash_fn  Pointer to user defined hash function of type ihash_hash_fn or
 *                 NULL to use default function
 * @return         Pointer to initialized hash table, or NULL on allocation failure
 *
 * @pre bucketsz > 0
 * @pre usersz + sizeof(icache_idx_t) >= keyoffs + sizeof(ssize_t)
 *
 * @post All slots are marked as empty (ICACHE_UNDEF)
 * @post Node pool is initialized as a freelist
 * @post chain_head == 0 (first node is free)
 *
 * @note The table is relocatable - all references are indices, not pointers
 */
icache *
icache_create_fn(size_t bucketsz, size_t chainsz, size_t keyoffs, size_t usersz, ihash_hash_fn hash_fn) {
    /* Allocate contiguous memory */
    icache *cache;

    if (bucketsz == 0)
        return NULL;    /* Error, impossible to init a hash without buckets */

    /* Allocate contiguous memory */
    cache = malloc(icache_get_required_memory_size(bucketsz, chainsz, usersz));

    return cache ? icache_init_fn(cache, bucketsz, chainsz, keyoffs, usersz, hash_fn) : NULL;
}

/**
 * @brief Initializes a hash table in pre-allocated memory
 *
 * Memory usage:
 * - Header: sizeof(icache) bytes
 * - Primary slots: entrysz * bucketsz bytes
 * - Node pool: entrysz * chainsz bytes
 *
 * Initialization:
 * 1. All primary slot entries have key = ICACHE_UNDEF, next = ICACHE_UNDEF
 * 2. Node pool entries form a linked list via 'next' indices
 * 3. chain_head points to first free node (0)
 * 4. Last node's next = ICACHE_UNDEF
 *
 * @param p        Previously allocated pointer where icache is going to be initialized
 * @param bucketsz Number of primary hash slots (must be > 0)
 * @param chainsz  Size of overflow node pool (can be 0)
 * @param keyoffs  Byte offset of 'key' field within entry
 * @param usersz   Total size of each user's entry in bytes
 * @param hash_fn  Pointer to user defined hash function of type ihash_hash_fn or
 *                 NULL to use default function
 * @return         Pointer to initialized hash table, or NULL on allocation failure
 *
 * @pre bucketsz > 0
 * @pre usersz + sizeof(icache_idx_t) >= keyoffs + sizeof(ssize_t)
 *
 * @post All slots are marked as empty (ICACHE_UNDEF)
 * @post Node pool is initialized as a freelist
 * @post chain_head == 0 (first node is free)
 *
 * @note The table is relocatable - all references are indices, not pointers.
 * @note It does not allocate memory, the caller must provide a pre-allocated buffer.
 * @note Use icache_get_required_memory_size macro to know number of bytes required for hash.
 */
icache *
icache_init_fn(void *p, size_t bucketsz, size_t chainsz, size_t keyoffs, size_t usersz, ihash_hash_fn hash_fn) {
    icache *cache = p;
    const size_t nodesz = usersz + sizeof(size_t);

    if (bucketsz == 0)
        return NULL;    /* Error, impossible to init a hash without buckets */

    cache->hashoffs = sizeof(icache);
    cache->listoffs = sizeof(icache) + ihash_get_required_memory_size(bucketsz, chainsz, usersz);
    cache->nodesz = nodesz;

    if (ihash_init_fn(p + cache->hashoffs, bucketsz, chainsz, keyoffs, nodesz, hash_fn) == NULL)
        return NULL;

    if (ilist2_init_fn(p + cache->listoffs, bucketsz + chainsz, sizeof(size_t)) == NULL)
        return NULL;

    return cache;
}

/**
 * @brief Clears all entries from the hash table
 *
 * Resets the hash table to empty state, as if it was just initialized.
 * All nodes are moved to the freelist for reuse.
 *
 * @param p         Pointer to hash table to clear
 * @param hash_fn   Pointer to user defined hash function of type ihash_hash_fn or
 *                  NULL to use default function
 *
 * @pre hash must be a valid, initialized hash table
 *
 * @post All slots are marked as empty (ICACHE_UNDEF)
 * @post Node pool is reinitialized as a freelist
 * @post chain_head points to first free node (bucketsz)
 *
 * @note Time complexity: O(bucketsz + chainsz)
 * @warning Does NOT call any destructor on stored values
 * @warning All previously returned pointers become invalid
 *
 * @code
 * struct MyEntry *hash = icache_create(hash, 16, 32);
 * icache_put(hash, 42, 100);
 * icache_put(hash, 43, 200);
 *
 * icache_clear(hash);  // hash becomes empty
 *
 * assert(icache_get(hash, 42) == NULL);
 * @endcode
 */
void
icache_clear(void *p, ihash_hash_fn hash_fn) {
    icache *cache = p;

    ihash_clear(p + cache->hashoffs, hash_fn);
    ilist2_clear(p + cache->listoffs);
}

/**
 * @brief Inserts or updates an entry in the hash table
 *
 * Algorithm:
 * 1. Compute hash index
 * 2. Check primary slot:
 *    a) If empty, insert directly into primary slot
 *    b) If key matches, update existing entry
 *    c) If collision, traverse chain to find key or end
 * 3. If key not found and end of chain reached:
 *    a) Allocate a node from the freelist
 *    b) Link it at the end of the chain
 *    c) Copy value into the new node
 *
 * @param hash      Pointer to hash table
 * @param key       Key to insert/update
 * @return          Pointer to entry (existing or new), or NULL if no free nodes
 *
 * @pre key != ICACHE_UNDEF
 * @pre value != NULL
 * @pre hash is properly initialized
 *
 * @retval non-NULL Pointer to the entry (inserted or updated)
 * @retval NULL     Node pool exhausted (chains reached with no free nodes)
 *
 * @note This function does NOT copy any value - only manages key and next links
 * @note The value field is left untouched (caller must fill it)
 */
void *
icache_touch_fn(icache *cache, ssize_t key) {
    const size_t idxoffs = cache->nodesz - sizeof(size_t);
    ihash *hash = (void *)cache + cache->hashoffs;
    size_t *list = (void *)cache + cache->listoffs;
    void *e = icache_get(cache, key);

    if (e != NULL)
        ilist2_move_front_by_idx(list, *(size_t *)(e + idxoffs));
    else {
        e = ihash_touch_fn(hash, key);

        if (e != NULL)
            *(ssize_t *)(e + idxoffs) = (ssize_t)ilist2_put_front(list, key);
        else {
            ssize_t lru_key = ilist2_pop_back(list);
            ihash_erase(hash, lru_key);
            return ihash_touch_fn(hash, key);
        }
    }

    return e;
}

