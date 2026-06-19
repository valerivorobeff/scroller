/**
 * @file icache.h
 * @brief Index-based lru cache implementation with fixed-size entries
 *
 * @note This code uses GNU extensions (void* arithmetic) and requires
 *  GCC, Clang, or compatible compiler.
 *
 * This module provides a lightweight, memory-efficient lru cache that stores
 * entries in a contiguous memory block. Unlike traditional caches that
 * use pointers, this implementation uses indices
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
 *
 * @author Your Name
 * @date 2026-06-04
 * @version 1.0
 *
 */

/**
 * @page example_icache Example: Using Index-Based Hash Table
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
 * struct CacheEntry *cache = icache_create(cache, 16, 32);
 * if (!cache) {
 *     perror("Failed to create hash table");
 *     exit(1);
 * }
 *
 * // Insert a value
 * ssize_t key = 42;
 * char value[] = "Hello, World!";
 * struct CacheEntry *entry = icache_put(cache, key, value);
 * if (!entry) {
 *     printf("Hash table is full!\n");
 * }
 *
 * // Retrieve a value
 * entry = icache_get(cache, 42);
 * if (entry) {
 *     printf("Found: %s\n", entry->data);
 * }
 *
 * // Clean up
 * icache_free(cache);
 * @endcode
 *
 * @section example2 Shared Memory Usage
 *
 * @code
 * // Create hash table in shared memory for IPC
 * int shm_fd = shm_open("/my_hash", O_CREAT | O_RDWR, 0666);
 * size_t total_size = sizeof(icache) + entrysz * (bucketsz + chainsz);
 * ftruncate(shm_fd, total_size);
 *
 * struct CacheEntry *shared_cache = mmap(NULL, total_size,
 *                                        PROT_READ | PROT_WRITE,
 *                                        MAP_SHARED, shm_fd, 0);
 *
 * // Initialize (only once, in one process)
 * static int initialized = 0;
 * if (!initialized) {
 *     shared_cache = icache_create(shared_hash, 16, 32);
 *     initialized = 1;
 * }
 * @endcode
 */

#ifndef _ICACHE_H_
#define _ICACHE_H_

#include "ihash.h"

/**
 * @struct icache
 * @brief Header structure for the index-based lru cache
 *
 * This structure precedes the actual cache data in memory.
 * The memory layout is:
 * ```
 * [icache header][hash table entries][lru list entries]
 * ```
 *
 * @var icache:hashoffs
 *      Offset of inner ihash structure and data
 *      Number of slots in the hash table (primary bucket array)
 * @var icache::listoffs
 *      Offset of inner ilist2 structure and data
 * @var icache::nodesz
 *      Total size of full entry (incl. internal data)
 *
 * @note The actual entries follow this header in memory
 * @see icache_create_fn()
 */
typedef struct icache {
    size_t hashoffs;        /** Offset of hash */
    size_t listoffs;        /** Offset of list */
    size_t nodesz;          /** Node size, usersz + sizeof(size_t) */
} icache;

/**
 * @def icache_create(h, bucketsz_, chainsz_, hash_fn_)
 * @brief Creates a new hash table for a specific entry type
 *
 * This macro automatically computes field offsets using typeof() and
 * offsetof(), eliminating the need for manual offset calculation.
 *
 * @param h         Pointer to a variable that will receive the cache pointer
 * @param bucketsz_ Number of slots in the cache (primary bucket count)
 * @param chainsz_  Number of slots in the chains table (overflow nodes)
 * @param hash_fn_  Pointer to user defined hash function of type ihash_hash_fn or
 *                  NULL to use default function
 * @return          Pointer to the created cache (cast to type of h)
 *
 * @pre The entry structure must have 'key' field
 * @pre 'key' field must be of type ssize_t
 *
 * @code
 * struct MyEntry {
 *     ssize_t key;
 *     int value;
 * };
 * struct MyEntry *cache = icache_create(cache, 16, 32, NULL);
 * @endcode
 *
 * @see icache_create_fn()
 */
#define icache_create(h, bucketsz_, chainsz_, hash_fn_) \
    (typeof(h))icache_create_fn( \
        bucketsz_, chainsz_, \
        offsetof(typeof(*h), key), \
        sizeof(*h), \
        hash_fn_)

/**
 * @def icache_init(h, bucketsz_, chainsz_, hash_fn_)
 * @brief Initializes a new cache for a specific entry type
 *
 * This macro automatically computes field offsets using typeof() and
 * offsetof(), eliminating the need for manual offset calculation.
 *
 * @param p         Previously allocated pointer where icache is going to be initialized
 * @param h         Pointer to a variable that will receive the cache pointer
 * @param bucketsz_ Number of slots in the cache (primary bucket count)
 * @param chainsz_  Number of slots in the chains table (overflow nodes)
 * @param hash_fn_  Pointer to user defined hash function of type ihash_hash_fn or
 *                  NULL to use default function
 * @return          Pointer to the created cache (cast to type of h)
 *
 * @pre The entry structure must have 'key' field
 * @pre 'key' field must be of type ssize_t
 *
 * @code
 * struct MyEntry {
 *     ssize_t key;
 *     int value;
 * };
 * struct MyEntry *cache = icache_init(cache, 16, 32, NULL);
 * @endcode
 *
 * @see icache_init_fn()
 */
#define icache_init(p, h, bucketsz_, chainsz_, hash_fn_) \
    (typeof(h))icache_init_fn( \
        p, bucketsz_, chainsz_, \
        offsetof(typeof(*h), key), \
        sizeof(*h), \
        hash_fn_)

/**
 * @brief Clears all entries from the cache
 *
 * Resets the cache to empty state, as if it was just initialized.
 *
 * @param p         Pointer to cache to clear
 * @param hash_fn   Pointer to user defined hash function of type ihash_hash_fn or
 *                  NULL to use default function
 *
 * @warning All previously returned pointers become invalid
 */
void icache_clear(void *p, ihash_hash_fn hash_fn);

/**
 * @brief Frees a cache created with icache_create()
 *
 * @param cache Pointer to the cache to free
 *
 * @note This function simply calls free() on the provided pointer
 * @warning Does NOT free any external data pointed to by entries
 */
void icache_free(void *p);

/**
 * @def icache_get(h, key_)
 * @brief Retrieves an entry from the cache by key
 *
 * @param h    Pointer to the cache
 * @param key_ Key to search for (ssize_t)
 * @return     Pointer to the matching entry, or NULL if not found
 *
 * @note The returned pointer has the same type as h
 * @see icache_get_fn()
 */
#define icache_get(h, key_) \
    ((typeof(h))ihash_get_fn((ihash *)((void *)h + ((icache *)(h))->hashoffs), key_))

/**
 * @def icache_exists(h, key_)
 * @brief Checks if a key exists in the cache
 *
 * @param h      Pointer to the cache
 * @param key_   Key to check
 * @return       1 if key exists, 0 otherwise
 *
 * @code
 * struct MyEntry *cache = icache_create(cache, 16, 32);
 * icache_put(cache, 42, 100);
 *
 * if (icache_exists(cache, 42)) {
 *     printf("Key 42 exists\n");
 * }
 * @endcode
 */
#define icache_exists(h, key_) \
    (icache_get(h, key_) != NULL)

/**
 * @def icache_get_member_ptr(h, key_, member)
 * @brief Retrieves a pointer to a specific member of an entry by key
 *
 * @param h      Pointer to the cache
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
 * struct MyEntry *cache = icache_create(cache, 16, 32);
 * icache_put(cache, 42, 100);
 *
 * int *val = icache_get_member_ptr(cache, 42, value);
 * if (val) printf("value: %d\n", *val);
 * @endcode
 */
#define icache_get_member_ptr(h, key_, member) \
    ({ \
        typeof(h) _e = icache_get(h, key_); \
        _e ? &_e->member : NULL; \
    })

/**
 * @def icache_get_member(h, key_, member)
 * @brief Retrieves a copy of a specific member value by key
 *
 * @param h      Pointer to the cache
 * @param key_   Key to search for (ssize_t)
 * @param member Member name within the entry struct
 * @return       Copy of the member, or zero-initialized if not found
 *
 * @warning Returns a copy, not a pointer. For large structs, use icache_get_member_ptr()
 * @warning If key not found, returns zero-initialized value (may be indistinguishable from actual zero)
 *
 * @code
 * struct MyEntry {
 *     ssize_t key;
 *     int value;
 * };
 * struct MyEntry *cache = icache_create(cache, 16, 32);
 * icache_put(cache, 42, 100);
 *
 * int val = icache_get_member(cache, 42, value);
 * printf("value: %d\n", val);
 * @endcode
 */
#define icache_get_member(h, key_, member) \
    ({ \
        typeof(h) _e = icache_get(h, key_); \
        _e ? _e->member : (typeof(_e->member)){0}; \
    })

/**
 * @def icache_get_value_ptr(h, key_)
 * @brief Retrieves a pointer to value of an entry by key
 *
 * @param h      Pointer to the cache
 * @param key_   Key to search for (ssize_t)
 * @return       Pointer to the value, or NULL if key not found
 *
 * @code
 * struct MyEntry {
 *     ssize_t key;
 *     int value;
 *     char name[32];
 * };
 * struct MyEntry *cache = icache_create(cache, 16, 32);
 * icache_put(cache, 42, 100);
 *
 * int *val = icache_get_value_ptr(cache, 42);
 * if (val) printf("value: %d\n", *val);
 * @endcode
 */
#define icache_get_value_ptr(h, key_) \
    icache_get_member_ptr(h, key_, value)

/**
 * @def icache_get_value(h, key_)
 * @brief Retrieves a copy of value by key
 *
 * @param h      Pointer to the cache
 * @param key_   Key to search for (ssize_t)
 * @return       Copy of the value, or zero-initialized if not found
 *
 * @warning Returns a copy, not a pointer. For large structs, use icache_get_value_ptr()
 * @warning If key not found, returns zero-initialized value (may be indistinguishable from actual zero)
 *
 * @code
 * struct MyEntry {
 *     ssize_t key;
 *     int value;
 * };
 * struct MyEntry *cache = icache_create(cache, 16, 32);
 * icache_put(cache, 42, 100);
 *
 * int val = icache_get_value(cache, 42);
 * printf("value: %d\n", val);
 * @endcode
 */
#define icache_get_value(h, key_) \
    icache_get_member(h, key_, value)

/**
 * @def icache_put(h, key_, value_)
 * @brief Inserts or updates an entry in the cache
 *
 * If the key already exists, updates the existing entry.
 * If the key is new, allocates a new entry from the node pool.
 *
 * @param h      Pointer to the cache
 * @param key_   Key to insert/update (ssize_t)
 * @param value_ Value to store (by copy, size determined by sizeof(value_))
 * @return       Pointer to the entry (existing or newly allocated)
 *
 * @pre value_ must be of the same type as the 'value' field in the entry
 *
 * @warning If the node pool is exhausted, it erases the least resently used (LRU)
 * node from the cache and puts the new node there
 * @see icache_touch_fn()
 */
#define icache_put(h, key_, value_) \
    ({ \
        typeof(h) _e = (typeof(h))icache_touch_fn((icache *)h, key_); \
        if (_e) { \
            _e->value = (value_); \
        } \
        _e; \
    })

/**
 * @def icache_put_key(h, key_)
 * @brief Inserts a key without initializing the value (useful for sets)
 *
 * @param h      Pointer to the cache
 * @param key_   Key to insert
 * @return       Pointer to the entry
 *
 * @note The entry's value field is left uninitialized
 * @warning Useful for hash sets where only keys matter
 *
 * @code
 * struct SetEntry {
 *     ssize_t key;
 *     // no value field!
 * };
 * struct SetEntry *set = icache_create(set, 16, 32);
 * icache_put_key(set, 42);
 *
 * if (icache_get(set, 42)) {
 *     printf("Key exists in set\n");
 * }
 * @endcode
 */
#define icache_put_key(h, key_) \
    (typeof(h))icache_touch_fn((icache *)h, key_)

/**
 * @def icache_put_struct(h, struct_ptr)
 * @brief Inserts or updates an entry by copying entire user struct
 *
 * @param h          Pointer to the cache
 * @param struct_ptr Pointer to user struct to copy
 * @return           Pointer to the entry
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
 * struct MyEntry *cache = icache_create(cache, 16, 32);
 *
 * struct MyEntry data = {42, 100, "Alice"};
 * icache_put_struct(cache, &data);
 * @endcode
 */
#define icache_put_struct(h, struct_ptr) \
    ({ \
        typeof(h) _e = icache_put_key(h, (struct_ptr)->key); \
        if (_e) { \
            *_e = *(struct_ptr); \
        } \
        _e; \
    })

/**
 * @def icache_get_required_memory_size
 * @brief Utility macro to find out the number of bytes required for cache.
 *
 * @param bucketsz  Number of slots in the cache (primary bucket count)
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
 * size_t size = icache_get_required_memory_size(16, 32, sizeof(MyEntry));
 * MyEntry *cache = malloc(size);
 * cache = icache_init(icache, 16, 32);
 * icache_put(hash, 42, 100);
 * @endcode
 */
#define icache_get_required_memory_size(bucketsz, chainsz, usersz) \
    (sizeof(icache) + \
        ihash_get_required_memory_size(bucketsz, chainsz, usersz) + \
        ilist2_get_required_memory_size(bucketsz + chainsz, sizeof(size_t)) \
    )

/**
 * @cond PRIVATE
 * Inner functions and macros - implementation details
 * @endcond
 */

/**
 * @brief Internal function for cache creation
 *
 * @param bucketsz Number of primary hash slots
 * @param chainsz  Number of chain hash slots
 * @param keyoffs  Byte offset of 'key' field within entry
 * @param usersz   Total size of each user's entry in bytes
 * @param hash_fn  Pointer to user defined hash function of type ihash_hash_fn or
 *                 NULL to use default function
 * @return         Pointer to initialized cache, or NULL on allocation failure
 *
 * @note Allocates memory with malloc
 * @see icache_create macro
 */
icache *icache_create_fn(size_t bucketsz, size_t chainsz, size_t keyoffs, size_t usersz, ihash_hash_fn hash_fn);

/**
 * @brief Internal function for cache initialization
 *
 * @param p        Previously allocated pointer where icache is going to be initialized
 * @param bucketsz Number of primary hash slots
 * @param chainsz  Number of chain hash slots
 * @param keyoffs  Byte offset of 'key' field within entry
 * @param usersz   Total size of each user's entry in bytes
 * @param hash_fn  Pointer to user defined hash function of type ihash_hash_fn or
 *                 NULL to use default function
 * @return         Pointer to initialized cache, or NULL on allocation failure
 *
 * @see icache_init macro
 * @see icache_get_required_memory_size
 */
icache *icache_init_fn(void *p, size_t bucketsz, size_t chainsz, size_t keyoffs, size_t usersz, ihash_hash_fn hash_fn);

/**
 * @brief Internal function for key insertion/update
 *
 * @param cache     Pointer to cache
 * @param key       Key to insert/update
 * @return          Pointer to entry
 */
void *icache_touch_fn(icache *cache, ssize_t key);

#endif /* _ICACHE_H_ */

