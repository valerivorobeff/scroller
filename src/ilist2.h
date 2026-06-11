/**
 * @file ilist2.h
 * @brief Index-based hash table implementation with fixed-size entries
 *
 * @note This code uses GNU extensions (void* arithmetic) and requires
 *  GCC, Clang, or compatible compiler.
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
 * @page example_ilist2 Example: Using Index-Based Hash Table
 *
 * @section example1 Basic Usage
 *
 * @code
 * // Define entry structure
 * struct CacheEntry {
 *     ssize_t key;    // Must be first? No, but offsets must match
 *     char data[256];
 * };
 *
 * // Create hash table with 16 primary slots and 32 overflow nodes
 * struct CacheEntry *cache = ilist2_create(cache, 16, 32);
 * if (!cache) {
 *     perror("Failed to create hash table");
 *     exit(1);
 * }
 *
 * // Insert a value
 * ssize_t key = 42;
 * char value[] = "Hello, World!";
 * struct CacheEntry *entry = ilist2_put(cache, key, value);
 * if (!entry) {
 *     printf("Hash table is full!\n");
 * }
 *
 * // Retrieve a value
 * entry = ilist2_get(cache, 42);
 * if (entry) {
 *     printf("Found: %s\n", entry->data);
 * }
 *
 * // Clean up
 * ilist2_free(cache);
 * @endcode
 *
 * @section example2 Shared Memory Usage
 *
 * @code
 * // Create hash table in shared memory for IPC
 * int shm_fd = shm_open("/my_hash", O_CREAT | O_RDWR, 0666);
 * size_t total_size = sizeof(ilist2) + entrysz * (bucketsz + chainsz);
 * ftruncate(shm_fd, total_size);
 *
 * struct CacheEntry *shared_hash = mmap(NULL, total_size,
 *                                        PROT_READ | PROT_WRITE,
 *                                        MAP_SHARED, shm_fd, 0);
 *
 * // Initialize (only once, in one process)
 * static int initialized = 0;
 * if (!initialized) {
 *     shared_hash = ilist2_create(shared_hash, 16, 32);
 *     initialized = 1;
 * }
 * @endcode
 */

#ifndef _ILIST2_H_
#define _ILIST2_H_

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
#define ILISTH_UNDEF (ssize_t)-1

/**
 * @struct ilist2
 * @brief Header structure for the index-based hash table
 *
 * This structure precedes the actual hash table data in memory.
 * The memory layout is:
 * ```
 * [ilist2 header][hash table entries][node pool entries]
 * ```
 *
 * @var ilist2::bucketsz
 *      Number of slots in the hash table (primary bucket array)
 * @var ilist2::chainsz
 *      Number of slots in the node pool chain
 * @var ilist2::chain_head
 *      Index of the first free node in the free node list, or IHASH_UNDEF if empty
 *
 * @note The actual entries follow this header in memory
 * @see ilist2_create_fn()
 */
typedef struct ilist2 {
    size_t listsz;          /** Number of primary hash buckets */
    ssize_t chain_head;     /** Head index of free node list (chains) */
    ssize_t back_idx;
    ssize_t front_idx;
    size_t nodesz;          /** Total size of full entry (incl. internal next) */
} ilist2;

/**
 * @def ilist2_create(h, bucketsz_, chainsz_, hash_fn_)
 * @brief Creates a new hash table for a specific entry type
 *
 * This macro automatically computes field offsets using typeof() and
 * offsetof(), eliminating the need for manual offset calculation.
 *
 * @param h         Pointer to a variable that will receive the hash table pointer
 * @param bucketsz_ Number of slots in the hash table (primary bucket count)
 * @param chainsz_  Number of slots in the chains table (overflow nodes)
 * @param hash_fn_  Pointer to user defined hash function of type ilist2_hash_fn or
 *                  NULL to use default function
 * @return          Pointer to the created hash table (cast to type of h)
 *
 * @pre The entry structure must have 'key' field
 * @pre 'key' field must be of type ssize_t
 *
 * @code
 * struct MyEntry {
 *     ssize_t key;
 *     int value;
 * };
 * struct MyEntry *hash = ilist2_create(hash, 16, 32, NULL);
 * @endcode
 *
 * @see ilist2_create_fn()
 */
#define ilist2_create(h, listsz_) \
    (typeof(h))ilist2_create_fn(listsz_, sizeof(*h))

/**
 * @def ilist2_init(h, bucketsz_, chainsz_, hash_fn_)
 * @brief Initializes a new hash table for a specific entry type
 *
 * This macro automatically computes field offsets using typeof() and
 * offsetof(), eliminating the need for manual offset calculation.
 *
 * @param p         Previously allocated pointer where ilist2 is going to be initialized
 * @param h         Pointer to a variable that will receive the hash table pointer
 * @param bucketsz_ Number of slots in the hash table (primary bucket count)
 * @param chainsz_  Number of slots in the chains table (overflow nodes)
 * @param hash_fn_  Pointer to user defined hash function of type ilist2_hash_fn or
 *                  NULL to use default function
 * @return          Pointer to the created hash table (cast to type of h)
 *
 * @pre The entry structure must have 'key' field
 * @pre 'key' field must be of type ssize_t
 *
 * @code
 * struct MyEntry {
 *     ssize_t key;
 *     int value;
 * };
 * struct MyEntry *hash = ilist2_init(hash, 16, 32, NULL);
 * @endcode
 *
 * @see ilist2_init_fn()
 */
#define ilist2_init(p, h, listsz_) \
    (typeof(h))ilist2_init_fn( p, listsz_, sizeof(*h))

/**
 * @brief Clears all entries from the hash table
 *
 * Resets the hash table to empty state, as if it was just initialized.
 *
 * @param p         Pointer to hash table to clear
 * @param hash_fn   Pointer to user defined hash function of type ilist2_hash_fn or
 *                  NULL to use default function
 *
 * @warning All previously returned pointers become invalid
 */
void ilist2_clear(void *p);

/**
 * @brief Frees a hash table created with ilist2_create()
 *
 * @param hash Pointer to the hash table to free
 *
 * @note This function simply calls free() on the provided pointer
 * @warning Does NOT free any external data pointed to by entries
 */
void ilist2_free(void *p);

/**
 * @def ilist2_get_back(h)
 * @brief Retrieves a pointer to value of an entry by key
 *
 * @param h      Pointer to the hash table
 * @param key_   Key to search for (ssize_t)
 * @return       Pointer to the value, or NULL if key not found
 *
 * @code
 * struct MyEntry {
 *     ssize_t key;
 *     int value;
 *     char name[32];
 * };
 * struct MyEntry *hash = ilist2_create(hash, 16, 32);
 * ilist2_put(hash, 42, 100);
 *
 * int *val = ilist2_get_value_ptr(hash, 42);
 * if (val) printf("value: %d\n", *val);
 * @endcode
 */
#define ilist2_get_back(h_) \
    ilist2_get_member_ptr(h, key_, value)

/**
 * @def ilist2_pop_back(h)
 * @brief Removes an entry from the hash table by key
 *
 * @param h      Pointer to the hash table
 * @param key_   Key to remove
 * @return       1 if entry was found and removed, 0 if key not found
 *
 * @code
 * struct MyEntry {
 *     ssize_t key;
 *     int value;
 * };
 * struct MyEntry *hash = ilist2_create(hash, 16, 32);
 *
 * ilist2_put(hash, 42, 100);
 * ilist2_erase(hash, 42);  // Removes the entry
 * @endcode
 */
#define ilist2_pop_back(h) \
    ilist2_erase_fn((ilist2 *)h, key_)

/**
 * @def ilist2_foreach(node, h)
 * @brief Iterates over all occupied entries in the hash table
 *
 * This macro provides a convenient way to traverse all non-empty entries
 * in the hash table, including both primary bucket slots and chain nodes.
 *
 * @param node  Name of the entry pointer variable (will be declared in the loop)
 * @param h     Pointer to the hash table (will be cast to ilist2*)
 *
 * @note The iterator visits entries in bucket order, then chain order
 * @warning Do NOT insert or erase entries while iterating
 * @warning The loop variable 'node' must not be modified inside the loop
 *
 * @code
 * struct MyEntry *entry;
 * ilist2_foreach(entry, hash) {
 *     printf("key: %ld, value: %d\n", entry->key, entry->value);
 * }
 * @endcode
 *
* @note _node_ is just used not to throw _bucket_idx_ out of the scope
 */
#define ilist2_foreach(node, h) \
    for (size_t _bucket_idx_ = 0, \
        _node_ __attribute__((unused)) = \
        (size_t)(node = ilist2_first_node_fn((ilist2 *)h, &_bucket_idx_)); \
        node; \
        node = ilist2_next_node_fn(node, (ilist2 *)h, &_bucket_idx_))

/**
 * @cond PRIVATE
 * Inner functions and macros - implementation details
 * @endcond
 */

/**
 * @cond PRIVATE
 * Inner functions and macros - implementation details
 * @endcond
 */

/**
 * @brief Internal type of ihash index
 */
typedef ssize_t ilist2_idx_t;

/**
 * @def ilist2_get_required_memory_size
 * @brief Utility macro to find out the number of bytes required for hash.
 *
 * @param bucketsz  Number of slots in the hash table (primary bucket count)
 * @param chainsz   Number of slots in the chains table (overflow nodes)
 * @param usersz   Total size of each user's entry in bytes
 *
 * @return          Number of bytes
 *
 * @code
 * struct MyEntry {
 *     ssize_t key;
 *     int value;
 * };
 * size_t size = ilist2_get_required_memory_size(16, 32, sizeof(MyEntry));
 * MyEntry *hash = malloc(size);
 * hash = hash_init(hash, hash, 16, 32);
 * ilist2_put(hash, 42, 100);
 * @endcode
 */
#define ilist2_get_required_memory_size(listsz, usersz) \
    ((usersz + sizeof(ilist2_idx_t) + sizeof(ilist2_idx_t)) * listsz + sizeof(ilist2))

/**
 * @brief Internal function for hash table creation
 *
 * @param bucketsz Number of primary hash slots
 * @param chainsz  Number of chain hash slots
 * @param keyoffs  Byte offset of 'key' field within entry
 * @param usersz   Total size of each user's entry in bytes
 * @param hash_fn  Pointer to user defined hash function of type ilist2_hash_fn or
 *                 NULL to use default function
 * @return         Pointer to initialized hash table, or NULL on allocation failure
 *
 * @note Allocates memory with malloc
 * @see ilist2_create macro
 */
ilist2 *ilist2_create_fn(size_t listsz, size_t usersz);

/**
 * @brief Internal function for hash table initialization
 *
 * @param p        Previously allocated pointer where ilist2 is going to be initialized
 * @param bucketsz Number of primary hash slots
 * @param chainsz  Number of chain hash slots
 * @param keyoffs  Byte offset of 'key' field within entry
 * @param usersz   Total size of each user's entry in bytes
 * @param hash_fn  Pointer to user defined hash function of type ilist2_hash_fn or
 *                 NULL to use default function
 * @return         Pointer to initialized hash table, or NULL on allocation failure
 *
 * @see ilist2_init macro
 * @see ilist2_get_required_memory_size
 */
ilist2 *ilist2_init_fn(void *p, size_t listsz, size_t usersz);

/**
 * @brief Internal function for key lookup
 *
 * @param hash     Pointer to hash table
 * @param key      Key to search for
 * @return         Pointer to entry, or NULL if not found
 *
 * @note It does not allocate memory, the caller must provide a pre-allocated buffer.
 */
void *ilist2_get_back_fn(ilist2 *list);

/**
 * @brief Internal function for key insertion/update
 *
 * @param hash      Pointer to hash table
 * @param key       Key to insert/update
 * @return          Pointer to entry, or NULL if node pool exhausted
 */
void *ilist2_pop_back_fn(ilist2 *list);

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
void ilist2_dump_simple(void *h);

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
void ilist2_dump_debug(void *h);

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
void ilist2_dump_freelist(void *h);

#else

#define ilist2_dump_simple(h)    ((void)0)
#define ilist2_dump_debug(h)     ((void)0)
#define ilist2_dump_freelist(h)  ((void)0)

#endif /* NDEBUG */

#endif /* _ILIST2_H_ */

