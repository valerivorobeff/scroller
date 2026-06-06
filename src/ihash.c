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
 * 1. A fixed-size array of primary slots (bucketsz)
 * 2. A pool of overflow nodes (chainsz)
 * 3. A freelist for node management
 *
 * Memory Layout:
 * ```
 * +-------------+
 * | ihash       | Header (bucketsz, chainsz, chain_head)
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
 * @see ihash.h
 */

/**
 * @todo:
 * 1. make ihash_getpv() to get pointer to value
 * 2. make ihash_getv() to get value (risky!)
 * 3. make ihash_putk() to add/update just the key
 * 4. make ihash_puts() to add/update the whole user's struct
 * 5. make ihash_exists() to check existence of a node
 * 6. make ihash_clear() to clear all the hash structures
 * 7. add variables keyoffs, usersz (or nodesz) to ihash struct and
 *      delete them from the following function arguments:
 *      ihash_get_fn, ihash_touch_fn, ihash_erase_fn
 * 8. make ihash_foreach()
 * 9. Investigate if it makes more sence to move all the entries including the first one
 *      into the chains pool and store in the buckets only the index of the first entry.
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
 * @brief Internal helper to get pointer to hash table primary buckets
 * @param h Pointer to hash table header
 * @return Pointer to the start of primary hash buckets
 */
#define ihash_get_buckets(h)   ((void *)((char *)h + sizeof(ihash)))

/**
 * @brief Internal helper to get pointer to chain pool
 * @param h Pointer to hash table header
 * @return Pointer to the start of chain pool
 */
#define ihash_get_chains(h) (ihash_get_buckets(h) + sizeof(h) * ((ihash*)(h))->bucketsz)

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
ihash *ihash_create_fn(size_t bucketsz, size_t chainsz, size_t keyoffs, size_t usersz);
ihash *ihash_init_fn(void *p, size_t bucketsz, size_t chainsz, size_t keyoffs, size_t usersz);
void *ihash_get_fn(ihash *hash, ssize_t key, size_t keyoffs, size_t usersz);
void *ihash_touch_fn(ihash *hash, ssize_t key, size_t keyoffs, size_t usersz);
int ihash_erase_fn(ihash *hash, ssize_t key, size_t keyoffs, size_t usersz);

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
 * - Primary slots: entrysz * bucketsz bytes
 * - Node pool: entrysz * chainsz bytes
 *
 * Initialization:
 * 1. All primary slot entries have key = IHASH_UNDEF, next = IHASH_UNDEF
 * 2. Node pool entries form a linked list via 'next' indices
 * 3. chain_head points to first free node (0)
 * 4. Last node's next = IHASH_UNDEF
 *
 * @param bucketsz Number of primary hash slots (must be > 0)
 * @param chainsz  Size of overflow node pool (can be 0)
 * @param keyoffs  Byte offset of 'key' field within entry
 * @param usersz   Total size of each user's entry in bytes
 * @return         Pointer to initialized hash table, or NULL on allocation failure
 *
 * @pre bucketsz > 0
 * @pre nodesz >= max(keyoffs, nextoffs) + sizeof(ssize_t)
 *
 * @post All slots are marked as empty (IHASH_UNDEF)
 * @post Node pool is initialized as a freelist
 * @post chain_head == 0 (first node is free)
 *
 * @note The table is relocatable - all references are indices, not pointers
 */
ihash *
ihash_create_fn(size_t bucketsz, size_t chainsz, size_t keyoffs, size_t usersz) {
    size_t nodesz = usersz + sizeof(ihash_idx_t);

    /* Allocate contiguous memory */
    ihash *hash = malloc(ihash_get_required_memory_size(bucketsz, chainsz, usersz));

    return hash ? ihash_init_fn(hash, bucketsz, chainsz, keyoffs, usersz) : NULL;
}

/**
 * @brief Initializes (doesn't alloc) a new hash table
 *
 * Memory usage:
 * - Header: sizeof(ihash) bytes
 * - Primary slots: entrysz * bucketsz bytes
 * - Node pool: entrysz * chainsz bytes
 *
 * Initialization:
 * 1. All primary slot entries have key = IHASH_UNDEF, next = IHASH_UNDEF
 * 2. Node pool entries form a linked list via 'next' indices
 * 3. chain_head points to first free node (0)
 * 4. Last node's next = IHASH_UNDEF
 *
 * @param p        Previously allocated pointer where ihash is going to be initialized
 * @param bucketsz Number of primary hash slots (must be > 0)
 * @param chainsz  Size of overflow node pool (can be 0)
 * @param keyoffs  Byte offset of 'key' field within entry
 * @param usersz   Total size of each user's entry in bytes
 * @return         Pointer to initialized hash table, or NULL on allocation failure
 *
 * @pre bucketsz > 0
 * @pre nodesz >= max(keyoffs, nextoffs) + sizeof(ssize_t)
 *
 * @post All slots are marked as empty (IHASH_UNDEF)
 * @post Node pool is initialized as a freelist
 * @post chain_head == 0 (first node is free)
 *
 * @note The table is relocatable - all references are indices, not pointers
 * @note It doesn't allocate memory, user should have made it him/herself before.
 * @note Use ihash_get_required_memory_size macro to know number of bytes required for hash.
 */
ihash *
ihash_init_fn(void *p, size_t bucketsz, size_t chainsz, size_t keyoffs, size_t usersz) {
    ihash *hash = p;
    size_t nodesz = usersz + sizeof(ihash_idx_t);
    size_t nextoffs = usersz;
    void *e;

    hash->bucketsz = bucketsz;
    hash->chainsz = chainsz;
    hash->chain_head = 0;
    e = ihash_get_buckets(hash);

    /* Initialize primary hash slots to empty state */
    for (size_t i = 0; i != bucketsz; ++i, e += nodesz) {
        *(ssize_t *)(e + keyoffs) = IHASH_UNDEF;
        *(ssize_t *)(e + nextoffs) = IHASH_UNDEF;
    }

    /* Initialize node pool as a linked list of free nodes */
    for (size_t i = 0; i != chainsz; ++i, e += nodesz) {
        *(ssize_t *)(e + keyoffs) = IHASH_UNDEF;
        *(ssize_t *)(e + nextoffs) = i + 1;  /* Point to next free node */
    }

    /* Mark the end of the freelist */
    if (chainsz > 0) {
        *(ssize_t *)(e - nodesz + nextoffs) = IHASH_UNDEF;
    }

    return hash;
}

/**
 * @brief Retrieves an entry by key
 *
 * Algorithm:
 * 1. Compute hash index = key % bucketsz
 * 2. Check the primary slot at that index
 * 3. If key matches, return entry
 * 4. If next = IHASH_UNDEF, key not found
 * 5. Otherwise, follow the chain of overflow nodes
 *
 * @param hash     Pointer to hash table
 * @param key      Key to search for (must not be IHASH_UNDEF)
 * @param keyoffs  Byte offset of 'key' field within entry
 * @param usersz   Total size of each user's entry in bytes
 * @return         Pointer to entry if found, NULL otherwise
 *
 * @pre key != IHASH_UNDEF
 * @pre hash is properly initialized
 *
 * @note Time complexity: O(1) average, O(n) worst-case
 * @warning Returns NULL if key is not present in the table
 */
void *
ihash_get_fn(ihash *hash, ssize_t key, size_t keyoffs, size_t usersz) {
    size_t nodesz = usersz + sizeof(ihash_idx_t);
    size_t nextoffs = usersz;
    size_t idx = hash_fn(key) % hash->bucketsz;         /* Compute hash bucket index */
    void *e = ihash_get_buckets(hash) + idx * nodesz;   /* buckets */
    void *nodes = ihash_get_chains(hash);               /* chains */

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
        e = nodes + *(ssize_t *)(e + nextoffs) * nodesz;
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
 * @param usersz   Total size of each user's entry in bytes
 * @return          Pointer to entry (existing or new), or NULL if no free nodes
 *
 * @pre key != IHASH_UNDEF
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
ihash_touch_fn(ihash *hash, ssize_t key, size_t keyoffs, size_t usersz) {
    size_t nodesz = usersz + sizeof(ihash_idx_t);
    size_t nextoffs = usersz;
    size_t idx = hash_fn(key) % hash->bucketsz;         /* Compute hash bucket index */
    void *e = ihash_get_buckets(hash) + idx * nodesz;   /* buckets */
    void *nodes = ihash_get_chains(hash);               /* chains */

    /* Case 1: Primary slot is empty - insert here */
    if (IHASH_UNDEF == *(ssize_t *)(e + keyoffs)) {
        *(ssize_t *)(e + keyoffs) = key;
        *(ssize_t *)(e + nextoffs) = IHASH_UNDEF;
        return e;
    }

    /* Case 2: Traverse chain to find key or end */
    for(;;) {
        if (key == *(ssize_t *)(e + keyoffs))
            return e;  /* Key exists - update will be done by caller */

        if (IHASH_UNDEF == *(ssize_t *)(e + nextoffs)) {
            /* Case 3: End of chain - need to allocate new node */
            if (hash->chain_head == IHASH_UNDEF)
                return NULL;  /* No free nodes available */

            /* Allocate from freelist */
            void *new_node = nodes + hash->chain_head * nodesz;
            *(ssize_t *)(e + nextoffs) = hash->chain_head;          /* Link to new node */
            hash->chain_head = *(ssize_t *)(new_node + nextoffs);   /* Update freelist head */

            /* Initialize the new node */
            *(ssize_t *)(new_node + keyoffs) = key;
            *(ssize_t *)(new_node + nextoffs) = IHASH_UNDEF;

            return new_node;
        }

        /* Move to next node in chain */
        e = nodes + *(ssize_t *)(e + nextoffs) * nodesz;
    }
}

/**
 * @brief Removes an entry from the hash table
 *
 * Algorithm:
 * 1. Find the bucket index
 * 2. Traverse the chain keeping track of previous node
 * 3. When found, unlink the node from the chain
 * 4. Add the freed node to the freelist
 *
 * Cases handled:
 * - Removing from the beginning of chain (bucket slot)
 * - Removing from middle of chain
 * - Removing from end of chain
 * - Key not found (returns 0)
 *
 * @param hash     Pointer to hash table
 * @param key      Key to remove
 * @param keyoffs  Offset of 'key' field
 * @param usersz   Total size of each user's entry in bytes
 * @return         1 if removed successfully, 0 if key not found
 *
 * @note The removed node is added to the freelist for reuse
 * @warning Does NOT call any destructor on the value field
 */
int
ihash_erase_fn(ihash *hash, ssize_t key, size_t keyoffs, size_t usersz) {
    size_t nodesz = usersz + sizeof(ihash_idx_t);
    size_t nextoffs = usersz;
    size_t idx = hash_fn(key) % hash->bucketsz;         /* Compute hash bucket index */
    void *e = ihash_get_buckets(hash) + idx * nodesz;   /* buckets */
    void *nodes = ihash_get_chains(hash);               /* chains */

    /* Case 1: Primary slot is empty - nothing to erase */
    if (IHASH_UNDEF == *(ssize_t *)(e + keyoffs))
        return 0;

    /* Case 2: Key is in bucket slot */
    if (key == *(ssize_t *)(e + keyoffs)) {
        ssize_t next_idx = *(ssize_t *)(e + nextoffs);

        if (IHASH_UNDEF == next_idx) {
            /* There is no chain, just clear the bucket */
            *(ssize_t *)(e + keyoffs) = IHASH_UNDEF;
            *(ssize_t *)(e + nextoffs) = IHASH_UNDEF;
        } else {
            void *first_node = nodes + next_idx * nodesz;
            /* Move the first node from chain to bucket */
            memcpy(e, first_node, nodesz);

            /* Move the previous node to top of free chain */
            *(ssize_t *)(first_node + keyoffs) = IHASH_UNDEF;
            *(ssize_t *)(first_node + nextoffs) = hash->chain_head;

            hash->chain_head = next_idx;
        }

        return 1;
    } else {
        /* Case 3: Traverse chain to find key or end */
        ssize_t cur_idx = *(ssize_t *)(e + nextoffs);

        while (IHASH_UNDEF != cur_idx) {
            void *prev = e;                 /* Remember previous node */
            e = nodes + cur_idx * nodesz;   /* Move to next node in chain */

            if (key == *(ssize_t *)(e + keyoffs)) {
                /* Key exists */

                /* Unlink current node (e) from the chain */
                ssize_t next_idx = *(ssize_t *)(e + nextoffs);
                *(ssize_t *)(prev + nextoffs) = next_idx;

                /* Move current node (e) to top of free chain */
                *(ssize_t *)(e + keyoffs) = IHASH_UNDEF;
                *(ssize_t *)(e + nextoffs) = hash->chain_head;

                hash->chain_head = cur_idx;

                return 1;
            }

            cur_idx = *(ssize_t *)(e + nextoffs);
        }

        /* We get here if no key was found, nothing to erase */
        return 0;
    }
}

