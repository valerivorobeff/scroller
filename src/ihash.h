/**
 * @file ihash.h
 * @brief Index-based hash table implementation with fixed-size entries
 *
 * This module provides a lightweight, memory-efficient hash table that stores
 * entries in a contiguous memory block. Unlike traditional hash tables that
 * use pointers, this implementation uses indices for next-node references,
 * making it suitable for:
 *
 * - Shared memory between processes (IPC)
 * - Memory-mapped files
 * - Embedded systems without dynamic allocation
 * - Persistent storage on disk
 *
 * @section features Key Features
 * - Index-based (pointer-free) design
 * - Separate chaining with node pool management
 * - Fixed-size entries for predictable memory usage
 * - Undefined value (-1) used as NULL equivalent
 * - Configurable key/value offsets within entries
 *
 * @note All node references are indices, making the table relocatable in memory
 * @warning IHASH_UNDEF (-1) cannot be used as a valid key value
 *
 * @author Your Name
 * @date 2026-06-04
 * @version 1.0
 *
 * @see grid.h For persistent storage integration
 */

/**
 * @page example_ihash Example: Using Index-Based Hash Table
 *
 * @section example1 Basic Usage
 *
 * @code
 * // Define entry structure
 * struct CacheEntry {
 *     ssize_t key;    // Must be first? No, but offsets must match
 *     ssize_t next;   // Required for chaining
 *     char data[256];
 * };
 *
 * // Create hash table with 16 primary slots and 32 overflow nodes
 * struct CacheEntry *cache = ihash_create(cache, 16, 32);
 * if (!cache) {
 *     perror("Failed to create hash table");
 *     exit(1);
 * }
 *
 * // Insert a value
 * ssize_t key = 42;
 * char value[] = "Hello, World!";
 * struct CacheEntry *entry = ihash_put(cache, key, value);
 * if (!entry) {
 *     printf("Hash table is full!\n");
 * }
 *
 * // Retrieve a value
 * entry = ihash_get(cache, 42);
 * if (entry) {
 *     printf("Found: %s\n", entry->data);
 * }
 *
 * // Clean up
 * ihash_free(cache);
 * @endcode
 *
 * @section example2 Shared Memory Usage
 *
 * @code
 * // Create hash table in shared memory for IPC
 * int shm_fd = shm_open("/my_hash", O_CREAT | O_RDWR, 0666);
 * size_t total_size = sizeof(ihash) + entrysz * (size + cap);
 * ftruncate(shm_fd, total_size);
 *
 * struct CacheEntry *shared_hash = mmap(NULL, total_size, 
 *                                        PROT_READ | PROT_WRITE,
 *                                        MAP_SHARED, shm_fd, 0);
 *
 * // Initialize (only once, in one process)
 * static int initialized = 0;
 * if (!initialized) {
 *     shared_hash = ihash_create(shared_hash, 16, 32);
 *     initialized = 1;
 * }
 * @endcode
 */

#ifndef _IHASH_H_
#define _IHASH_H_

#include <stddef.h>
#include <sys/types.h>

/**
 * @def IHASH_UNDEF
 * @brief Sentinel value representing "no entry" or "end of chain"
 *
 * This value (-1) is used as:
 * - Empty slot marker in hash table entries
 * - Terminator for linked lists (next = -1)
 * - Invalid index for node allocation
 *
 * @warning Keys with value -1 are not supported
 */
#define IHASH_UNDEF (ssize_t)-1

/**
 * @struct ihash
 * @brief Header structure for the index-based hash table
 *
 * This structure precedes the actual hash table data in memory.
 * The memory layout is:
 * ```
 * [ihash header][hash table entries][node pool entries]
 * ```
 *
 * @var ihash::size
 *      Number of slots in the hash table (primary bucket array)
 * @var ihash::cap
 *      Capacity of the node pool (maximum number of overflow nodes)
 * @var ihash::head_node
 *      Index of the first free node in the freelist, or IHASH_UNDEF if empty
 *
 * @note The actual entries follow this header in memory
 * @see ihash_create_fn()
 */
typedef struct ihash {
    size_t size;         /** Number of primary hash slots */
    size_t cap;          /** Maximum number of overflow nodes */
    ssize_t head_node;   /** Head index of free node list */
} ihash;

/**
 * @def ihash_create(h, size_, cap_)
 * @brief Creates a new hash table for a specific entry type
 *
 * This macro automatically computes field offsets using typeof() and
 * offsetof(), eliminating the need for manual offset calculation.
 *
 * @param h      Pointer to a variable that will receive the hash table pointer
 * @param size_  Number of slots in the hash table (primary bucket count)
 * @param cap_   Capacity of the node pool (overflow nodes)
 * @return       Pointer to the created hash table (cast to type of h)
 *
 * @pre The entry structure must have 'key' and 'next' fields
 * @pre 'key' field must be of type ssize_t
 * @pre 'next' field must be of type ssize_t
 *
 * @code
 * struct MyEntry {
 *     ssize_t key;
 *     ssize_t next;
 *     int value;
 * };
 * struct MyEntry *hash = ihash_create(hash, 16, 32);
 * @endcode
 *
 * @see ihash_create_fn()
 */
#define ihash_create(h, size_, cap_) \
    (typeof(h))ihash_create_fn( \
        size_, cap_, \
        offsetof(typeof(*h), key), \
        offsetof(typeof(*h), next), \
        sizeof(*h))

/**
 * @brief Frees a hash table created with ihash_create()
 *
 * @param hash Pointer to the hash table to free
 *
 * @note This function simply calls free() on the provided pointer
 * @warning Does NOT free any external data pointed to by entries
 */
void ihash_free(void *hash);

/**
 * @def ihash_get(h, key_)
 * @brief Retrieves an entry from the hash table by key
 *
 * @param h    Pointer to the hash table
 * @param key_ Key to search for (ssize_t)
 * @return     Pointer to the matching entry, or NULL if not found
 *
 * @note The returned pointer has the same type as h
 * @see ihash_get_fn()
 */
#define ihash_get(h, key_) \
    (typeof(h))ihash_get_fn((ihash *)h, key_, offsetof(typeof(*h), key), offsetof(typeof(*h), next), sizeof(*h))

/**
 * @def ihash_put(h, key_, value_)
 * @brief Inserts or updates an entry in the hash table
 *
 * If the key already exists, updates the existing entry.
 * If the key is new, allocates a new entry from the node pool.
 *
 * @param h      Pointer to the hash table
 * @param key_   Key to insert/update (ssize_t)
 * @param value_ Value to store (by copy, size determined by sizeof(value_))
 * @return       Pointer to the entry (existing or newly allocated), 
 *               or NULL if no free nodes available
 *
 * @pre value_ must be of the same type as the 'value' field in the entry
 *
 * @note The entry's 'value' field is updated via memcpy()
 * @warning Returns NULL if the node pool is exhausted
 * @see ihash_put_fn()
 */
#define ihash_put(h, key_, value_) \
    ({ \
        typeof(h) _e = (typeof(h))ihash_put_fn((ihash *)h, key_, \
            offsetof(typeof(*h), key), \
            offsetof(typeof(*h), next), \
            sizeof(*h)); \
        if (_e) { \
            _e->value = (value_); \
        } \
        _e; \
    })

/**
 * @cond PRIVATE
 * Inner functions and macros - implementation details
 * @endcond
 */

/**
 * @brief Internal function for hash table creation
 *
 * @param size     Number of primary hash slots
 * @param cap      Node pool capacity
 * @param keyoffs  Byte offset of 'key' field within entry
 * @param nextoffs Byte offset of 'next' field within entry
 * @param entrysz  Total size of each entry in bytes
 * @return         Pointer to initialized hash table
 *
 * @note Allocates memory: sizeof(ihash) + entrysz * (size + cap)
 * @see ihash_create macro
 */
ihash *ihash_create_fn(size_t size, size_t cap, size_t keyoffs,
        size_t nextoffs, size_t entrysz);

/**
 * @brief Internal function for key lookup
 *
 * @param hash     Pointer to hash table
 * @param key      Key to search for
 * @param keyoffs  Byte offset of 'key' field within entry
 * @param nextoffs Byte offset of 'next' field within entry
 * @param entrysz  Total size of each entry
 * @return         Pointer to entry, or NULL if not found
 */
void *ihash_get_fn(ihash *hash, ssize_t key, size_t keyoffs, size_t nextoffs, size_t entrysz);

/**
 * @brief Internal function for key insertion/update
 *
 * @param hash      Pointer to hash table
 * @param key       Key to insert/update
 * @param keyoffs   Byte offset of 'key' field
 * @param nextoffs  Byte offset of 'next' field
 * @param entrysz   Total size of each entry
 * @return          Pointer to entry, or NULL if node pool exhausted
 */
void *ihash_put_fn(ihash *hash, ssize_t key, size_t keyoffs, size_t nextoffs, size_t entrysz);

#endif /* _IHASH_H_ */

