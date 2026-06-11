/**
 * @file ilist2.c
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
 * | ilist2       | Header (bucketsz, chainsz, chain_head)
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
 * @see ilist2.h
 */

/**
 * @todo:
 * 1. correct ilist2_foreach()
 * 2. Think of interface.
 *      ilist2_get* and ilist2_put* macro names don't match
 *      now: ilist2_get -> ilist2_put_struct maybe: ilist2_get_struct, ilist2_put_struct
 *      ilist2_get_member_ptr and ilist2_get_member don't have their analogs of ilist2_put*s
 * 3. Investigate if it makes more sence to move all the entries including the first one
 *      into the chains pool and store in the buckets only the index of the first entry.
 */

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
#define ilist2_get_prev_offs(h)   ((void *)((char *)h + sizeof(ilist2)))

/**
 * @brief Internal helper to get pointer to chain pool
 * @param h Pointer to hash table header
 * @return Pointer to the start of chain pool
 */
#define ilist2_get_next_offs(h) ((char *)ilist2_get_buckets(h) + ((ilist2 *)(h))->nodesz * ((ilist2*)(h))->bucketsz)

/**
 * @cond PRIVATE
 * Public functions - forward declarations
 * @endcond
 */

void ilist2_free(void *list);
ilist2 *ilist2_create_fn(size_t listsz, size_t usersz);
ilist2 *ilist2_init_fn(void *p, size_t list, size_t usersz);
void ilist2_clear(void *p);
void *ilist2_get_back_fn(ilist2 *list);
void *ilist2_pop_back_fn(ilist2 *list);
int ilist2_erase_fn(ilist2 *list, void *node);

#ifndef NDEBUG
void ilist2_dump_simple(void *h);
void ilist2_dump_debug(void *h);
void ilist2_dump_freelist(void *h);
#endif /* NDEBUG */

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
ilist2_free(void *list) {
    free((ilist2 *)list);
}

/**
 * @brief Creates and initializes a new hash table
 *
 * Memory allocation:
 * - Header: sizeof(ilist2) bytes
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
 * @param hash_fn  Pointer to user defined hash function of type ilist2_hash_fn or
 *                 NULL to use default function
 * @return         Pointer to initialized hash table, or NULL on allocation failure
 *
 * @pre bucketsz > 0
 * @pre usersz + sizeof(ilist2_idx_t) >= keyoffs + sizeof(ssize_t)
 *
 * @post All slots are marked as empty (IHASH_UNDEF)
 * @post Node pool is initialized as a freelist
 * @post chain_head == 0 (first node is free)
 *
 * @note The table is relocatable - all references are indices, not pointers
 */
ilist2 *
ilist2_create_fn(size_t listsz, size_t usersz) {
    /* Allocate contiguous memory */
    ilist2 *list;

    /* Allocate contiguous memory */
    list = malloc(ilist2_get_required_memory_size(listsz, usersz));

    return list ? ilist2_init_fn(list, listsz, usersz) : NULL;
}

/**
 * @brief Initializes a hash table in pre-allocated memory
 *
 * Memory usage:
 * - Header: sizeof(ilist2) bytes
 * - Primary slots: entrysz * bucketsz bytes
 * - Node pool: entrysz * chainsz bytes
 *
 * Initialization:
 * 1. All primary slot entries have key = IHASH_UNDEF, next = IHASH_UNDEF
 * 2. Node pool entries form a linked list via 'next' indices
 * 3. chain_head points to first free node (0)
 * 4. Last node's next = IHASH_UNDEF
 *
 * @param p        Previously allocated pointer where ilist2 is going to be initialized
 * @param bucketsz Number of primary hash slots (must be > 0)
 * @param chainsz  Size of overflow node pool (can be 0)
 * @param keyoffs  Byte offset of 'key' field within entry
 * @param usersz   Total size of each user's entry in bytes
 * @param hash_fn  Pointer to user defined hash function of type ilist2_hash_fn or
 *                 NULL to use default function
 * @return         Pointer to initialized hash table, or NULL on allocation failure
 *
 * @pre bucketsz > 0
 * @pre usersz + sizeof(ilist2_idx_t) >= keyoffs + sizeof(ssize_t)
 *
 * @post All slots are marked as empty (IHASH_UNDEF)
 * @post Node pool is initialized as a freelist
 * @post chain_head == 0 (first node is free)
 *
 * @note The table is relocatable - all references are indices, not pointers.
 * @note It does not allocate memory, the caller must provide a pre-allocated buffer.
 * @note Use ilist2_get_required_memory_size macro to know number of bytes required for hash.
 */
ilist2 *
ilist2_init_fn(void *p, size_t listsz, size_t usersz) {
    const size_t nodesz = usersz + sizeof(ilist2_idx_t) + sizeof(ilist2_idx_t);
    ilist2 *list = p + nodesz * listsz; /* struct ilist2 is int the end of data */

    list->listsz = listsz;
    list->nodesz = nodesz;

    ilist2_clear(p);

    return list;
}

/**
 * @brief Clears all entries from the hash table
 *
 * Resets the hash table to empty state, as if it was just initialized.
 * All nodes are moved to the freelist for reuse.
 *
 * @param p         Pointer to hash table to clear
 * @param hash_fn   Pointer to user defined hash function of type ilist2_hash_fn or
 *                  NULL to use default function
 *
 * @pre hash must be a valid, initialized hash table
 *
 * @post All slots are marked as empty (IHASH_UNDEF)
 * @post Node pool is reinitialized as a freelist
 * @post chain_head points to first free node (bucketsz)
 *
 * @note Time complexity: O(bucketsz + chainsz)
 * @warning Does NOT call any destructor on stored values
 * @warning All previously returned pointers become invalid
 *
 * @code
 * struct MyEntry *hash = ilist2_create(hash, 16, 32);
 * ilist2_put(hash, 42, 100);
 * ilist2_put(hash, 43, 200);
 *
 * ilist2_clear(hash);  // hash becomes empty
 *
 * assert(ilist2_get(hash, 42) == NULL);
 * @endcode
 */
void
ilist2_clear(void *p) {
    ilist2 *list = p + nodesz * listsz; /* struct ilist2 is int the end of data */
    const size_t nodesz = list->nodesz;
    const size_t listsz = list->listsz;
    const size_t nextoffs = nodesz - sizeof(ilist2_idx_t);
    const size_t prevoffs = nextoffs - 1;
    void *e = p;

    list->back_idx =    ILIST_UNDEF;
    list->front_idx =   ILIST_UNDEF;

    /* Initialize primary hash slots to empty state */
    for (size_t i = 0; i != listsz; ++i, e += nodesz) {
        *(ssize_t *)(e + prevoffs) = ILIST_UNDEF;
        *(ssize_t *)(e + nextoffs) = i + 1;
    }

    /* Mark the end of the freelist */
    if (listsz > 0) {
        list->chain_head = 0;
        *(ssize_t *)(e - nodesz + nextoffs) = ILIST_UNDEF;
    } else
        hash->chain_head = IHASH_UNDEF;
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
 * @return         Pointer to entry if found, NULL otherwise
 *
 * @pre key != IHASH_UNDEF
 * @pre hash is properly initialized
 *
 * @note Time complexity: O(1) average, O(n) worst-case
 * @warning Returns NULL if key is not present in the table
 */
void *
ilist2_get_back_fn(ilist2 *p) {
    const size_t nodesz = list->nodesz;
    const size_t listsz = list->listsz;
    ilist2 *list = p + nodesz * listsz; /* struct ilist2 is int the end of data */
    const ssize_t back_idx = list->back_idx;

    return back_idx != ILIST_UNDEF ? (void *)p + back_idx * nodesz : NULL;
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
ilist2_put_back_fn(ilist2 *list, void *value) {
    const size_t nodesz = hash->nodesz;
    const size_t keyoffs = hash->keyoffs;
    const size_t nextoffs = nodesz - sizeof(ilist2_idx_t);
    const size_t idx = hash->hash_fn(key) % hash->bucketsz; /* Compute hash bucket index */
    void *e = ilist2_get_buckets(hash) + idx * nodesz;       /* buckets */
    void *chains = ilist2_get_chains(hash);                  /* chains */

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
ilist2_erase_fn(ilist2 *hash, ssize_t key) {
    const size_t nodesz = hash->nodesz;
    const size_t keyoffs = hash->keyoffs;
    const size_t nextoffs = nodesz - sizeof(ilist2_idx_t);
    const size_t idx = hash->hash_fn(key) % hash->bucketsz; /* Compute hash bucket index */
    void *e = ilist2_get_buckets(hash) + idx * nodesz;       /* buckets */
    void *chains = ilist2_get_chains(hash);                  /* chains */

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
            void *first_node = chains + next_idx * nodesz;
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
            e = chains + cur_idx * nodesz;  /* Move to next node in chain */

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

/**
 * @brief Returns the first occupied entry in the hash table
 *
 * Scans buckets sequentially from index 0 and returns the first
 * slot that contains a valid key (not IHASH_UNDEF).
 *
 * @param hash       Pointer to hash table
 * @param bucket_idx Pointer to store the bucket index of the found entry
 * @return           Pointer to the first occupied entry, or NULL if table is empty
 *
 * @post bucket_idx is set to the index of the found bucket, or bucketsz if empty
 * @see ilist2_next_node_fn()
 * @see ilist2_foreach
 */
void *
ilist2_first_node_fn(ilist2 *hash, size_t *bucket_idx) {
    const size_t nodesz = hash->nodesz;
    const size_t keyoffs = hash->keyoffs;
    const size_t bucketsz = hash->bucketsz;
    void *e = ilist2_get_buckets(hash);

    for (size_t i = 0; i != bucketsz; ++i, e += nodesz) {
        if (IHASH_UNDEF != *(ssize_t *)(e + keyoffs)) {
            *bucket_idx = i;
            return e;
        }
    }

    *bucket_idx = bucketsz;
    return NULL;    /* Hash is empty */
}

/**
 * @brief Returns the next occupied entry after the given node
 *
 * Implements depth-first traversal:
 * 1. If current node has a chain, returns the first node in the chain
 * 2. Otherwise, scans subsequent buckets for the next occupied slot
 *
 * @param node       Current node pointer (must be from previous call)
 * @param hash       Pointer to hash table
 * @param bucket_idx Pointer to current bucket index (updated on each call)
 * @return           Pointer to next occupied entry, or NULL if no more entries
 *
 * @pre node must be a valid pointer returned by ilist2_first_node_fn() or previous ilist2_next_node_fn()
 * @post bucket_idx is updated to the bucket index of the returned entry
 * @see ilist2_first_node_fn()
 * @see ilist2_foreach
 */
void *
ilist2_next_node_fn(void *node, ilist2 *hash, size_t *bucket_idx) {
    const size_t nodesz = hash->nodesz;
    const size_t keyoffs = hash->keyoffs;
    const size_t nextoffs = nodesz - sizeof(ilist2_idx_t);
    void *buckets = ilist2_get_buckets(hash);                    /* buckets */
    const size_t bucketsz = hash->bucketsz;

    assert((node - buckets) % nodesz == 0);

    if (node == NULL)
        return NULL;

    for(;;) {
        if (IHASH_UNDEF != *(ssize_t *)(node + nextoffs))
            return buckets + *(ssize_t *)(node + nextoffs) * nodesz; /* Return next node in chain */
        else {
            ++*bucket_idx;                                      /* Move to the next bucket */
            if (*bucket_idx >= bucketsz)
                return NULL;                                    /* No more buckets */

            node = buckets + *bucket_idx * nodesz;              /* Move to the top of the next bucket */

            if (IHASH_UNDEF != *(ssize_t *)(node + keyoffs))    /* Next node found in bucket */
                return node;
            else {
                /* Find next occupied bucket */
                for (++*bucket_idx; *bucket_idx < bucketsz; ++*bucket_idx) {
                    node += nodesz;
                    if (IHASH_UNDEF != *(ssize_t *)(node + keyoffs))
                        return node;
                }

                return NULL;                                    /* No more buckets */
            }
        }
    }
}

#ifndef NDEBUG

/**
 * @brief Prints a simple summary of the hash table
 *
 * Displays each bucket and its chain of keys in a human-readable format.
 * Empty buckets are marked as EMPTY.
 *
 * @param h Pointer to hash table
 *
 * @code
 * === HASH TABLE (buckets=4, chains=8, freelist=3) ===
 *   [  0] key=   4
 *   [  1] key=   1 -> 5
 *   [  2] EMPTY
 *   [  3] key=   3 -> 11 -> 7
 * =================================
 * @endcode
 *
 * @see ilist2_dump_debug()
 * @see ilist2_dump_freelist()
 */
void
ilist2_dump_simple(void *h) {
    ilist2 *hash = h;
    const size_t nodesz = hash->nodesz;
    const size_t keyoffs = hash->keyoffs;
    const size_t nextoffs = nodesz - sizeof(ilist2_idx_t);
    const size_t total_buckets = hash->bucketsz;
    char *buckets = (char *)ilist2_get_buckets(hash);
    char *chains = (char *)ilist2_get_chains(hash);

    printf("\n=== HASH TABLE (buckets=%zu, chains=%zu, freelist=%zd) ===\n",
           hash->bucketsz, hash->chainsz, hash->chain_head);

    for (size_t i = 0; i < total_buckets; ++i) {
        char *bucket = buckets + i * nodesz;
        ssize_t key = *(ssize_t *)(bucket + keyoffs);
        ssize_t next = *(ssize_t *)(bucket + nextoffs);

        if (key == IHASH_UNDEF) {
            printf("  [%3zu] EMPTY\n", i);
        } else {
            printf("  [%3zu] key=%4ld", i, key);

            /* Traverse chain */
            ssize_t chain_idx = next;
            while (chain_idx != IHASH_UNDEF) {
                char *chain_node = chains + chain_idx * nodesz;
                ssize_t chain_key = *(ssize_t *)(chain_node + keyoffs);
                printf(" -> %ld", chain_key);
                chain_idx = *(ssize_t *)(chain_node + nextoffs);
            }
            printf("\n");
        }
    }
    printf("=================================\n");
}

/**
 * @brief Prints detailed debug information about the hash table
 *
 * Displays all slots (buckets + chains) with their key and next values,
 * showing internal indices and freelist state. Useful for debugging
 * hash table corruption or freelist issues.
 *
 * @param h Pointer to hash table
 *
 * @note Output includes key offsets, node sizes, and complete freelist chain
 * @see ilist2_dump_simple()
 * @see ilist2_dump_freelist()
 */
void
ilist2_dump_debug(void *h) {
    ilist2 *hash = h;
    const size_t nodesz = hash->nodesz;
    const size_t keyoffs = hash->keyoffs;
    const size_t nextoffs = nodesz - sizeof(ilist2_idx_t);
    const size_t total_buckets = hash->bucketsz;
    const size_t total_nodes = hash->bucketsz + hash->chainsz;
    char *buckets = (char *)ilist2_get_buckets(hash);
    char *chains = (char *)ilist2_get_chains(hash);

    printf("\n=== HASH TABLE DEBUG ===\n");
    printf("bucketsz=%zu, chainsz=%zu, total_slots=%zu\n",
           hash->bucketsz, hash->chainsz, total_nodes);
    printf("keyoffs=%zu, nextoffs=%zu, nodesz=%zu\n",
           keyoffs, nextoffs, nodesz);
    printf("freelist_head=%zd\n\n", hash->chain_head);

    /* Dump all slots (buckets + chains) */
    for (size_t i = 0; i < total_nodes; ++i) {
        char *slot = buckets + i * nodesz;
        ssize_t key = *(ssize_t *)(slot + keyoffs);
        ssize_t next = *(ssize_t *)(slot + nextoffs);
        const char *type = (i < hash->bucketsz) ? "BUCKET" : "CHAIN";

        if (key == IHASH_UNDEF) {
            printf("%s[%3zu]: EMPTY (next=%zd)\n", type, i, next);
        } else {
            printf("%s[%3zu]: key=%4ld (next=%zd)\n", type, i, key, next);
        }
    }

    /* Dump freelist */
    if (hash->chain_head != IHASH_UNDEF) {
        printf("\nFreelist: ");
        ssize_t idx = hash->chain_head;
        while (idx != IHASH_UNDEF && idx < (ssize_t)total_nodes) {
            printf("%zd", idx);
            char *node = chains + idx * nodesz;
            idx = *(ssize_t *)(node + nextoffs);
            printf(" -> ");
            if (idx == hash->chain_head) break; /* loop detection */
        }
        printf("IHASH_UNDEF\n");
    }

    printf("========================\n");
}

/**
 * @brief Prints the freelist chain of the hash table
 *
 * Displays the linked list of free nodes available for reuse,
 * along with the total count of free nodes.
 *
 * @param h Pointer to hash table
 *
 * @note Valid freelist indices are in range [0, chainsz-1]
 * @warning If an index is out of range, prints an error message
 *
 * @code
 * Freelist: 3 -> 1 -> 0 -> IHASH_UNDEF, Freelist contains 3 nodes
 * @endcode
 *
 * @see ilist2_dump_simple()
 * @see ilist2_dump_debug()
 */
void ilist2_dump_freelist(void *h) {
    ilist2 *hash = h;
    ssize_t count = 0;
    ssize_t idx = hash->chain_head;
    const size_t nodesz = hash->nodesz;
    const size_t nextoffs = nodesz - sizeof(ilist2_idx_t);
    void *chains = ilist2_get_chains(hash);

    printf("Freelist: ");
    while (idx != IHASH_UNDEF) {
        if (idx < 0 || idx >= hash->chainsz) {
            printf("ERROR: freelist index %zd out of range\n", idx);
            break;
        }
        printf("%zd -> ", idx);
        count++;
        if (count > 1000) break;
        char *node = chains + idx * hash->nodesz;
        idx = *(ssize_t *)(node + nextoffs);
    }
    printf("IHASH_UNDEF, ");
    printf("Freelist contains %zd nodes\n", count);
}

#endif /* NDEBUG */

