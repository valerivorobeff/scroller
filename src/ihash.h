/**
 * @file ihash.h
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
 * @page example_ihash Example: Using Index-Based Hash Table
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
 * size_t total_size = sizeof(ihash) + entrysz * (bucketsz + chainsz);
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
 * @brief ihash hash function prototype
 *
 * User can define his/her own hash function of this type when
 * initializing the hash
 *
 * @param key   Key
 * @return      Hash sum
 */
typedef size_t (*ihash_hash_fn)(size_t key);

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
 * @var ihash::bucketsz
 *      Number of slots in the hash table (primary bucket array)
 * @var ihash::chainsz
 *      Number of slots in the node pool chain
 * @var ihash::chain_head
 *      Index of the first free node in the free node list, or IHASH_UNDEF if empty
 *
 * @note The actual entries follow this header in memory
 * @see ihash_create_fn()
 */
typedef struct ihash {
    size_t bucketsz;        /** Number of primary hash buckets */
    size_t chainsz;         /** Maximum number of overflow nodes in all the chains */
    ssize_t chain_head;     /** Head index of free node list (chains) */
    size_t keyoffs;         /** Offset of 'key' field within full entry */
    size_t nodesz;          /** Total size of full entry (incl. internal next) */
    ihash_hash_fn hash_fn;  /** Pointer to hash function */
} ihash;

/**
 * @def ihash_create(h, bucketsz_, chainsz_, hash_fn_)
 * @brief Creates a new hash table for a specific entry type
 *
 * This macro automatically computes field offsets using typeof() and
 * offsetof(), eliminating the need for manual offset calculation.
 *
 * @param h         Pointer to a variable that will receive the hash table pointer
 * @param bucketsz_ Number of slots in the hash table (primary bucket count)
 * @param chainsz_  Number of slots in the chains table (overflow nodes)
 * @param hash_fn_  Pointer to user defined hash function of type ihash_hash_fn or
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
 * struct MyEntry *hash = ihash_create(hash, 16, 32, NULL);
 * @endcode
 *
 * @see ihash_create_fn()
 */
#define ihash_create(h, bucketsz_, chainsz_, hash_fn_) \
    (typeof(h))ihash_create_fn( \
        bucketsz_, chainsz_, \
        offsetof(typeof(*h), key), \
        sizeof(*h), \
        hash_fn_)

/**
 * @def ihash_init(h, bucketsz_, chainsz_, hash_fn_)
 * @brief Initializes a new hash table for a specific entry type
 *
 * This macro automatically computes field offsets using typeof() and
 * offsetof(), eliminating the need for manual offset calculation.
 *
 * @param p         Previously allocated pointer where ihash is going to be initialized
 * @param h         Pointer to a variable that will receive the hash table pointer
 * @param bucketsz_ Number of slots in the hash table (primary bucket count)
 * @param chainsz_  Number of slots in the chains table (overflow nodes)
 * @param hash_fn_  Pointer to user defined hash function of type ihash_hash_fn or
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
 * struct MyEntry *hash = ihash_init(hash, 16, 32, NULL);
 * @endcode
 *
 * @see ihash_init_fn()
 */
#define ihash_init(p, h, bucketsz_, chainsz_, hash_fn_) \
    (typeof(h))ihash_init_fn( \
        p, bucketsz_, chainsz_, \
        offsetof(typeof(*h), key), \
        sizeof(*h), \
        hash_fn_)

/**
 * @brief Clears all entries from the hash table
 *
 * Resets the hash table to empty state, as if it was just initialized.
 *
 * @param p         Pointer to hash table to clear
 * @param hash_fn   Pointer to user defined hash function of type ihash_hash_fn or
 *                  NULL to use default function
 *
 * @warning All previously returned pointers become invalid
 */
void ihash_clear(void *p, ihash_hash_fn hash_fn);

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
    ((typeof(h))ihash_get_fn((ihash *)h, key_))

/**
 * @def ihash_exists(h, key_)
 * @brief Checks if a key exists in the hash table
 *
 * @param h      Pointer to the hash table
 * @param key_   Key to check
 * @return       1 if key exists, 0 otherwise
 *
 * @code
 * struct MyEntry *hash = ihash_create(hash, 16, 32);
 * ihash_put(hash, 42, 100);
 *
 * if (ihash_exists(hash, 42)) {
 *     printf("Key 42 exists\n");
 * }
 * @endcode
 */
#define ihash_exists(h, key_) \
    (ihash_get(h, key_) != NULL)

/**
 * @def ihash_get_member_ptr(h, key_, member)
 * @brief Retrieves a pointer to a specific member of an entry by key
 *
 * @param h      Pointer to the hash table
 * @param key_   Key to search for (ssize_t)
 * @param member Member name within the entry struct (e.g., value, data, etc.)
 * @return       Pointer to the specified member, or NULL if key not found
 *
 * @code
 * struct MyEntry {
 *     ssize_t key;
 *     int value;
 *     char name[32];
 * };
 * struct MyEntry *hash = ihash_create(hash, 16, 32);
 * ihash_put(hash, 42, 100);
 *
 * int *val = ihash_get_member_ptr(hash, 42, value);
 * if (val) printf("value: %d\n", *val);
 * @endcode
 */
#define ihash_get_member_ptr(h, key_, member) \
    ({ \
        typeof(h) _e = ihash_get(h, key_); \
        _e ? &_e->member : NULL; \
    })

/**
 * @def ihash_get_member(h, key_, member)
 * @brief Retrieves a copy of a specific member value by key
 *
 * @param h      Pointer to the hash table
 * @param key_   Key to search for (ssize_t)
 * @param member Member name within the entry struct
 * @return       Copy of the member, or zero-initialized if not found
 *
 * @warning Returns a copy, not a pointer. For large structs, use ihash_get_member_ptr()
 * @warning If key not found, returns zero-initialized value (may be indistinguishable from actual zero)
 *
 * @code
 * struct MyEntry {
 *     ssize_t key;
 *     int value;
 * };
 * struct MyEntry *hash = ihash_create(hash, 16, 32);
 * ihash_put(hash, 42, 100);
 *
 * int val = ihash_get_member(hash, 42, value);
 * printf("value: %d\n", val);
 * @endcode
 */
#define ihash_get_member(h, key_, member) \
    ({ \
        typeof(h) _e = ihash_get(h, key_); \
        _e ? _e->member : (typeof(_e->member)){0}; \
    })

/**
 * @def ihash_get_value_ptr(h, key_)
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
 * struct MyEntry *hash = ihash_create(hash, 16, 32);
 * ihash_put(hash, 42, 100);
 *
 * int *val = ihash_get_value_ptr(hash, 42);
 * if (val) printf("value: %d\n", *val);
 * @endcode
 */
#define ihash_get_value_ptr(h, key_) \
    ihash_get_member_ptr(h, key_, value)

/**
 * @def ihash_get_value(h, key_)
 * @brief Retrieves a copy of value by key
 *
 * @param h      Pointer to the hash table
 * @param key_   Key to search for (ssize_t)
 * @return       Copy of the value, or zero-initialized if not found
 *
 * @warning Returns a copy, not a pointer. For large structs, use ihash_get_value_ptr()
 * @warning If key not found, returns zero-initialized value (may be indistinguishable from actual zero)
 *
 * @code
 * struct MyEntry {
 *     ssize_t key;
 *     int value;
 * };
 * struct MyEntry *hash = ihash_create(hash, 16, 32);
 * ihash_put(hash, 42, 100);
 *
 * int val = ihash_get_value(hash, 42);
 * printf("value: %d\n", val);
 * @endcode
 */
#define ihash_get_value(h, key_) \
    ihash_get_member(h, key_, value)

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
 * @see ihash_touch_fn()
 */
#define ihash_put(h, key_, value_) \
    ({ \
        typeof(h) _e = (typeof(h))ihash_touch_fn((ihash *)h, key_); \
        if (_e) { \
            _e->value = (value_); \
        } \
        _e; \
    })

/**
 * @def ihash_put_key(h, key_)
 * @brief Inserts a key without initializing the value (useful for sets)
 *
 * @param h      Pointer to the hash table
 * @param key_   Key to insert
 * @return       Pointer to the entry, or NULL if no free nodes available
 *
 * @note The entry's value field is left uninitialized
 * @warning Useful for hash sets where only keys matter
 *
 * @code
 * struct SetEntry {
 *     ssize_t key;
 *     // no value field!
 * };
 * struct SetEntry *set = ihash_create(set, 16, 32);
 * ihash_put_key(set, 42);
 *
 * if (ihash_get(set, 42)) {
 *     printf("Key exists in set\n");
 * }
 * @endcode
 */
#define ihash_put_key(h, key_) \
    (typeof(h))ihash_touch_fn((ihash *)h, key_)

/**
 * @def ihash_put_struct(h, struct_ptr)
 * @brief Inserts or updates an entry by copying entire user struct
 *
 * @param h          Pointer to the hash table
 * @param struct_ptr Pointer to user struct to copy
 * @return           Pointer to the entry, or NULL if no free nodes available
 *
 * @note Copies the entire user struct (including key) into the entry
 * @warning There must be The key field of type size_t in struct_ptr
 *
 * @code
 * struct MyEntry {
 *     ssize_t key;
 *     int value;
 *     char name[32];
 * };
 * struct MyEntry *hash = ihash_create(hash, 16, 32);
 *
 * struct MyEntry data = {42, 100, "Alice"};
 * ihash_put_struct(hash, &data);
 * @endcode
 */
#define ihash_put_struct(h, struct_ptr) \
    ({ \
        typeof(h) _e = ihash_put_key(h, (struct_ptr)->key); \
        if (_e) { \
            *_e = *(struct_ptr); \
        } \
        _e; \
    })

/**
 * @def ihash_erase(h, key_)
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
 * struct MyEntry *hash = ihash_create(hash, 16, 32);
 *
 * ihash_put(hash, 42, 100);
 * ihash_erase(hash, 42);  // Removes the entry
 * @endcode
 */
#define ihash_erase(h, key_) \
    ihash_erase_fn((ihash *)h, key_)

/**
 * @def ihash_foreach(node, h)
 * @brief Iterates over all occupied entries in the hash table
 *
 * This macro provides a convenient way to traverse all non-empty entries
 * in the hash table, including both primary bucket slots and chain nodes.
 *
 * @param node  Name of the entry pointer variable (will be declared in the loop)
 * @param h     Pointer to the hash table (will be cast to ihash*)
 *
 * @note The iterator visits entries in bucket order, then chain order
 * @warning Do NOT insert or erase entries while iterating
 * @warning The loop variable 'node' must not be modified inside the loop
 *
 * @code
 * struct MyEntry *entry;
 * ihash_foreach(entry, hash) {
 *     printf("key: %ld, value: %d\n", entry->key, entry->value);
 * }
 * @endcode
 *
* @note _node_ is just used not to throw _bucket_idx_ out of the scope
 */
#define ihash_foreach(node, h) \
    for (size_t _bucket_idx_ = 0, \
        _node_ __attribute__((unused)) = \
        (size_t)(node = ihash_first_node_fn((ihash *)h, &_bucket_idx_)); \
        node; \
        node = ihash_next_node_fn(node, (ihash *)h, &_bucket_idx_))

/**
 * @cond PRIVATE
 * Inner functions and macros - implementation details
 * @endcond
 */

/**
 * @brief Internal type of ihash index
 */
typedef ssize_t ihash_idx_t;

/**
 * @def ihash_get_required_memory_size
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
 * size_t size = ihash_get_required_memory_size(16, 32, sizeof(MyEntry));
 * MyEntry *hash = malloc(size);
 * hash = hash_init(hash, hash, 16, 32);
 * ihash_put(hash, 42, 100);
 * @endcode
 */
#define ihash_get_required_memory_size(bucketsz, chainsz, usersz) \
    (sizeof(ihash) + (usersz + sizeof(ihash_idx_t)) * (bucketsz + chainsz))

/**
 * @brief Internal function for hash table creation
 *
 * @param bucketsz Number of primary hash slots
 * @param chainsz  Number of chain hash slots
 * @param keyoffs  Byte offset of 'key' field within entry
 * @param usersz   Total size of each user's entry in bytes
 * @param hash_fn  Pointer to user defined hash function of type ihash_hash_fn or
 *                 NULL to use default function
 * @return         Pointer to initialized hash table, or NULL on allocation failure
 *
 * @note Allocates memory with malloc
 * @see ihash_create macro
 */
ihash *ihash_create_fn(size_t bucketsz, size_t chainsz, size_t keyoffs, size_t usersz, ihash_hash_fn hash_fn);

/**
 * @brief Internal function for hash table initialization
 *
 * @param p        Previously allocated pointer where ihash is going to be initialized
 * @param bucketsz Number of primary hash slots
 * @param chainsz  Number of chain hash slots
 * @param keyoffs  Byte offset of 'key' field within entry
 * @param usersz   Total size of each user's entry in bytes
 * @param hash_fn  Pointer to user defined hash function of type ihash_hash_fn or
 *                 NULL to use default function
 * @return         Pointer to initialized hash table, or NULL on allocation failure
 *
 * @see ihash_init macro
 * @see ihash_get_required_memory_size
 */
ihash *ihash_init_fn(void *p, size_t bucketsz, size_t chainsz, size_t keyoffs, size_t usersz, ihash_hash_fn hash_fn);

/**
 * @brief Internal function for key lookup
 *
 * @param hash     Pointer to hash table
 * @param key      Key to search for
 * @return         Pointer to entry, or NULL if not found
 *
 * @note It does not allocate memory, the caller must provide a pre-allocated buffer.
 */
void *ihash_get_fn(ihash *hash, ssize_t key);

/**
 * @brief Internal function for key insertion/update
 *
 * @param hash      Pointer to hash table
 * @param key       Key to insert/update
 * @return          Pointer to entry, or NULL if node pool exhausted
 */
void *ihash_touch_fn(ihash *hash, ssize_t key);

/**
 * @brief Erases an entry from the hash table by key
 *
 * @param hash     Pointer to hash table
 * @param key      Key to remove
 * @return         1 if entry was found and removed, 0 if key not found
 */
int ihash_erase_fn(ihash *hash, ssize_t key);

/**
 * @brief Returns the first occupied entry in the hash table
 *
 * @param hash       Pointer to hash table
 * @param bucket_idx Pointer to store the bucket index of the found entry
 * @return           Pointer to the first occupied entry, or NULL if table is empty
 *
 * @see ihash_next_node_fn()
 * @see ihash_foreach
 */
void *ihash_first_node_fn(ihash *hash, size_t *bucket_idx);

/**
 * @brief Returns the next occupied entry after the given node
 *
 * @param node       Current node pointer (must be from previous call)
 * @param hash       Pointer to hash table
 * @param bucket_idx Pointer to current bucket index (updated on each call)
 * @return           Pointer to next occupied entry, or NULL if no more entries
 *
 * @see ihash_first_node_fn()
 * @see ihash_foreach
 */
void *ihash_next_node_fn(void *node, ihash *hash, size_t *bucket_idx);

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
 * @see ihash_dump_debug()
 * @see ihash_dump_freelist()
 */
void ihash_dump_simple(void *h);

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
 * @see ihash_dump_simple()
 * @see ihash_dump_freelist()
 */
void ihash_dump_debug(void *h);

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
 * @see ihash_dump_simple()
 * @see ihash_dump_debug()
 */
void ihash_dump_freelist(void *h);

#else

#define ihash_dump_simple(h)    ((void)0)
#define ihash_dump_debug(h)     ((void)0)
#define ihash_dump_freelist(h)  ((void)0)

#endif /* NDEBUG */

#endif /* _IHASH_H_ */

