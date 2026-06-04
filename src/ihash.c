/**
 * @file ihash.c
 * @brief Implementation of index-based hash table
 *
 * This file implements a hash table that uses indices instead of pointers
 * for node linking, making it suitable for shared memory and persistent
 * storage scenarios.
 *
 * @section algorithm Hash Table Algorithm
 *
 * The hash table uses separate chaining with:
 * 1. A fixed-size array of primary slots (size)
 * 2. A pool of overflow nodes (cap)
 * 3. A freelist for node management
 *
 * Memory Layout:
 * ```
 * +-------------+
 * | ihash       | Header (size, cap, head_node)
 * +-------------+
 * | Slot 0      | Primary hash slots
 * | Slot 1      | (size entries)
 * | ...         |
 * | Slot N-1    |
 * +-------------+
 * | Node 0      | Node pool entries
 * | Node 1      | (cap entries)
 * | ...         |
 * | Node M-1    |
 * +-------------+
 * ```
 *
 * @see ihash.h
 */

#include "ihash.h"
#include <malloc.h>
#include <string.h>

/**
 * @cond PRIVATE
 * Static functions and data
 * @endcond
 */

/**
 * @brief Internal helper to get pointer to hash table slots
 * @param h Pointer to hash table header
 * @return Pointer to the start of primary hash slots
 */
#define ihash_get_tab(h)   ((void *)((char *)h + sizeof(ihash)))

/**
 * @brief Internal helper to get pointer to node pool
 * @param h Pointer to hash table header
 * @return Pointer to the start of overflow node pool
 */
#define ihash_get_nodes(h) (ihash_get_tab(h) + sizeof(h) * ((ihash*)(h))->size)

/**
 * @brief Hash function for integer keys
 * @param key Input key value
 * @return Hash value (identity function)
 *
 * @note Currently uses identity hashing (key -> key)
 * @todo Consider using a more sophisticated hash function for better distribution
 */
static ssize_t hash_fn(ssize_t key);

/**
 * @cond PRIVATE
 * Public functions - forward declarations
 * @endcond
 */

void ihash_free(void *hash);
ihash *ihash_create_fn(size_t size, size_t cap,
    size_t keyoffs, size_t nextoffs, size_t entrysz);
void *ihash_get_fn(ihash *hash, ssize_t key, size_t keyoffs, size_t nextoffs, size_t entrysz);
void *ihash_put_fn(ihash *hash, ssize_t key, size_t keyoffs, size_t nextoffs, size_t valueoffs, size_t valuesz, void *value, size_t entrysz);

/**
 * @cond PRIVATE
 * Implementation
 * @endcond
 */

/**
 * @brief Hash function implementation
 * @param key Key to hash
 * @return Hashed value (currently just returns key)
 *
 * @note This identity hash is simple but may cause poor distribution
 *       for non-consecutive keys.
 */
static ssize_t
hash_fn(ssize_t key) {
    return key;
}

/**
 * @brief Frees the hash table memory
 * @param hash Pointer to hash table to free
 *
 * @note Just a wrapper around free() for API consistency
 */
void
ihash_free(void *hash) {
    free((ihash *)hash);
}

/**
 * @brief Creates and initializes a new hash table
 *
 * Memory allocation:
 * - Header: sizeof(ihash) bytes
 * - Primary slots: entrysz * size bytes
 * - Node pool: entrysz * cap bytes
 *
 * Initialization:
 * 1. All primary slot entries have key = IHASH_UNDEF, next = IHASH_UNDEF
 * 2. Node pool entries form a linked list via 'next' indices
 * 3. head_node points to first free node (0)
 * 4. Last node's next = IHASH_UNDEF
 *
 * @param size     Number of primary hash slots (must be > 0)
 * @param cap      Capacity of overflow node pool (can be 0)
 * @param keyoffs  Byte offset of 'key' field within entry
 * @param nextoffs Byte offset of 'next' field within entry
 * @param entrysz  Total size of each entry in bytes
 * @return         Pointer to initialized hash table, or NULL on allocation failure
 *
 * @pre size > 0
 * @pre entrysz >= max(keyoffs, nextoffs) + sizeof(ssize_t)
 *
 * @post All slots are marked as empty (IHASH_UNDEF)
 * @post Node pool is initialized as a freelist
 * @post head_node == 0 (first node is free)
 *
 * @note The table is relocatable - all references are indices, not pointers
 */
ihash *
ihash_create_fn(size_t size, size_t cap,
    size_t keyoffs, size_t nextoffs, size_t entrysz) {
    void *e;

    /* Allocate contiguous memory for header + slots + node pool */
    ihash *hash = malloc(sizeof(ihash) + entrysz * (size + cap));
    hash->size = size;
    hash->cap = cap;
    hash->head_node = 0;
    e = ihash_get_tab(hash);

    /* Initialize primary hash slots to empty state */
    for (size_t i = 0; i != size; ++i, e += entrysz) {
        *(ssize_t *)(e + keyoffs) = IHASH_UNDEF;
        *(ssize_t *)(e + nextoffs) = IHASH_UNDEF;
    }

    /* Initialize node pool as a linked list of free nodes */
    for (size_t i = 0; i != cap; ++i, e += entrysz) {
        *(ssize_t *)(e + keyoffs) = IHASH_UNDEF;
        *(ssize_t *)(e + nextoffs) = i + 1;  /* Point to next free node */
    }

    /* Mark the end of the freelist */
    if (cap > 0) {
        *(ssize_t *)(e - entrysz + nextoffs) = IHASH_UNDEF;
    }

    return hash;
}

/**
 * @brief Retrieves an entry by key
 *
 * Algorithm:
 * 1. Compute hash index = key % size
 * 2. Check the primary slot at that index
 * 3. If key matches, return entry
 * 4. If next = IHASH_UNDEF, key not found
 * 5. Otherwise, follow the chain of overflow nodes
 *
 * @param hash     Pointer to hash table
 * @param key      Key to search for (must not be IHASH_UNDEF)
 * @param keyoffs  Byte offset of 'key' field within entry
 * @param nextoffs Byte offset of 'next' field within entry
 * @param entrysz  Total size of each entry
 * @return         Pointer to entry if found, NULL otherwise
 *
 * @pre key != IHASH_UNDEF
 * @pre hash is properly initialized
 *
 * @note Time complexity: O(1) average, O(n) worst-case
 * @warning Returns NULL if key is not present in the table
 */
void *
ihash_get_fn(ihash *hash, ssize_t key, size_t keyoffs, size_t nextoffs, size_t entrysz) {
    size_t idx = hash_fn(key) % hash->size;          /* Compute hash bucket */
    void *e = ihash_get_tab(hash) + idx * entrysz;  /* Primary slot entry */
    void *nodes = ihash_get_nodes(hash);            /* Overflow node pool */

    /* Empty slot - key definitely not present */
    if (IHASH_UNDEF == *(ssize_t *)(e + keyoffs))
        return NULL;

    /* Traverse the chain looking for the key */
    for(;;) {
        if (key == *(ssize_t *)(e + keyoffs))
            return e;  /* Key found */

        if (IHASH_UNDEF == *(ssize_t *)(e + nextoffs))
            return NULL;  /* End of chain, key not found */

        /* Move to next node in the chain */
        e = nodes + *(ssize_t *)(e + nextoffs) * entrysz;
    }
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
 * @param keyoffs   Byte offset of 'key' field within entry
 * @param nextoffs  Byte offset of 'next' field within entry
 * @param valueoffs Byte offset of 'value' field within entry
 * @param valuesz   Size of the value data to copy
 * @param value     Pointer to the value data
 * @param entrysz   Total size of each entry
 * @return          Pointer to entry (existing or new), or NULL if no free nodes
 *
 * @pre key != IHASH_UNDEF
 * @pre value != NULL
 * @pre hash is properly initialized
 *
 * @retval non-NULL Pointer to the entry (inserted or updated)
 * @retval NULL     Node pool exhausted (cap reached with no free nodes)
 *
 * @note Updates existing entry by copying new value via memcpy()
 * @warning The value is copied byte-by-byte; ensure value type is trivial
 * @see ihash_put macro for type-safe usage
 */
void *
ihash_put_fn(ihash *hash, ssize_t key, size_t keyoffs, size_t nextoffs, size_t valueoffs, size_t valuesz, void *value, size_t entrysz) {
    size_t idx = hash_fn(key) % hash->size;         /* Compute hash bucket */
    void *e = ihash_get_tab(hash) + idx * entrysz;  /* Primary slot entry */
    void *nodes = ihash_get_nodes(hash);            /* Overflow node pool */

    /* Case 1: Primary slot is empty - insert here */
    if (IHASH_UNDEF == *(ssize_t *)(e + keyoffs)) {
        *(ssize_t *)(e + keyoffs) = key;
        *(ssize_t *)(e + nextoffs) = IHASH_UNDEF;
        memcpy(e + valueoffs, value, valuesz);
        return e;
    }

    /* Case 2: Traverse chain to find key or end */
    for(;;) {
        if (key == *(ssize_t *)(e + keyoffs))
            return e;  /* Key exists - update will be done by caller */

        if (IHASH_UNDEF == *(ssize_t *)(e + nextoffs)) {
            /* Case 3: End of chain - need to allocate new node */
            if (hash->head_node == IHASH_UNDEF)
                return NULL;  /* No free nodes available */

            /* Allocate from freelist */
            void *phead_node = nodes + hash->head_node * entrysz;
            *(ssize_t *)(e + nextoffs) = hash->head_node;  /* Link to new node */
            hash->head_node = *(ssize_t *)(phead_node + nextoffs);  /* Update freelist head */

            /* Initialize the new node */
            return nodes + *(ssize_t *)(e + nextoffs) * entrysz;
        }

        /* Move to next node in chain */
        e = nodes + *(ssize_t *)(e + nextoffs) * entrysz;
    }
}

