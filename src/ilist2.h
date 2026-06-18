/**
 * @file ilist2.h
 * @brief Index-based doubly linked list with fixed-size entries
 *
 * @note This code uses GNU extensions (void* arithmetic) and requires
 *  GCC, Clang, or compatible compiler.
 *
 * This module provides a lightweight, memory-efficient doubly linked list
 * that stores entries in a contiguous memory block. Unlike traditional
 * linked lists that use pointers, this implementation uses indices for
 * prev/next references, making it suitable for:
 *
 * - Shared memory between processes (IPC)
 * - Memory-mapped files
 * - Embedded systems without dynamic allocation
 * - Persistent storage on disk
 *
 * @section features Key Features
 * - Index-based (pointer-free) design
 * - O(1) push/pop at both ends
 * - O(1) move-to-front / move-to-back by index
 * - Fixed-size entries for predictable memory usage
 * - Undefined value (-1) used as NULL equivalent
 * - Relocatable in memory
 *
 * @note All node references are indices, making the list relocatable in memory
 * @warning ILIST2_UNDEF (-1) cannot be used as a valid index
 *
 * @author Your Name
 * @date 2026-06-17
 * @version 1.0
 */

#ifndef _ILIST2_H_
#define _ILIST2_H_

#include <sys/types.h>

/**
 * @def ILIST2_UNDEF
 * @brief Sentinel value representing "no entry" or "end of list"
 *
 * This value (-1) is used as:
 * - Empty slot marker in list entries
 * - Terminator for prev/next links (-1)
 * - Invalid index for node allocation
 *
 * @warning Indices with value -1 are not supported
 */
#define ILIST2_UNDEF (ssize_t)-1

/**
 * @typedef ilist2_idx_t
 * @brief Type for list indices (ssize_t)
 */
typedef ssize_t ilist2_idx_t;

/**
 * @struct ilist2
 * @brief Header structure for the index-based doubly linked list
 *
 * This structure precedes the actual list data in memory.
 * The memory layout is:
 * ```
 * [ilist2 header][node 0][node 1]...[node N-1]
 * ```
 *
 * Each node stores:
 * - User data (size = usersz)
 * - prev index (ilist2_idx_t)
 * - next index (ilist2_idx_t)
 *
 * @var ilist2::listsz
 *      Total number of slots in the list (maximum nodes)
 * @var ilist2::freelist_head
 *      Index of the first free node, or ILIST2_UNDEF if empty
 * @var ilist2::front_idx
 *      Index of the first node in the list, or ILIST2_UNDEF if empty
 * @var ilist2::back_idx
 *      Index of the last node in the list, or ILIST2_UNDEF if empty
 * @var ilist2::nodesz
 *      Total size of each node (usersz + prev + next)
 * @var ilist2::nodes
 *      Flexible array member containing all nodes
 *
 * @see ilist2_create_fn()
 */
typedef struct ilist2 {
    size_t listsz;
    ilist2_idx_t freelist_head;
    ilist2_idx_t front_idx;
    ilist2_idx_t back_idx;
    size_t nodesz;
    char nodes[];
} ilist2;

/**
 * @def ilist2_create(h, listsz_)
 * @brief Creates a new list for a specific entry type
 *
 * This macro automatically computes field offsets using typeof() and
 * sizeof(), eliminating the need for manual size calculation.
 *
 * @param h        Pointer to a variable that will receive the list pointer
 * @param listsz_  Number of slots in the list
 * @return         Pointer to the created list (cast to type of h)
 *
 * @code
 * struct MyEntry {
 *     int value;
 * };
 * struct MyEntry *list = ilist2_create(list, 16);
 * @endcode
 *
 * @see ilist2_create_fn()
 */
#define ilist2_create(h, listsz_) \
    (typeof(h))ilist2_create_fn(listsz_, sizeof(*h))

/**
 * @def ilist2_init(h, listsz_)
 * @brief Initializes a list in pre-allocated memory
 *
 * @param h        Previously allocated pointer where list is going to be initialized
 * @param listsz_  Number of slots in the list
 * @return         Pointer to the initialized list (cast to type of h)
 *
 * @pre h must point to a memory region of at least ilist2_get_required_memory_size()
 *
 * @code
 * struct MyEntry *list = malloc(ilist2_get_required_memory_size(16, sizeof(MyEntry)));
 * list = ilist2_init(list, 16);
 * @endcode
 *
 * @see ilist2_init_fn()
 * @see ilist2_get_required_memory_size
 */
#define ilist2_init(h, listsz_) \
    (typeof(h))ilist2_init_fn(h, listsz_, sizeof(*h))

/**
 * @brief Clears all entries from the list
 *
 * Resets the list to empty state, as if it was just initialized.
 * All nodes are moved to the freelist for reuse.
 *
 * @param p Pointer to list to clear
 *
 * @post All slots are marked as empty
 * @post freelist_head points to first free node (0)
 * @post front_idx == ILIST2_UNDEF
 * @post back_idx == ILIST2_UNDEF
 *
 * @warning Does NOT call any destructor on stored values
 * @warning All previously returned pointers become invalid
 *
 * @code
 * struct MyEntry *list = ilist2_create(list, 16);
 * ilist2_put_back(list, 42);
 * ilist2_clear(list);  // list becomes empty
 * assert(ilist2_empty(list));
 * @endcode
 */
void ilist2_clear(void *p);

/**
 * @brief Frees a list created with ilist2_create()
 *
 * @param p Pointer to the list to free
 *
 * @note This function simply calls free() on the provided pointer
 * @warning Does NOT free any external data pointed to by entries
 */
void ilist2_free(void *p);

/**
 * @def ilist2_get_back(h)
 * @brief Retrieves the last element from the list
 *
 * @param h Pointer to the list
 * @return  Value of the last element, or 0 if list is empty
 *
 * @warning Returns 0 if list is empty (may be indistinguishable from actual 0)
 * @see ilist2_empty()
 */
#define ilist2_get_back(h) \
    ((typeof(*h))(*(typeof(h))ilist2_get_back_fn((ilist2 *)h)))

/**
 * @def ilist2_get_front(h)
 * @brief Retrieves the first element from the list
 *
 * @param h Pointer to the list
 * @return  Value of the first element, or 0 if list is empty
 *
 * @warning Returns 0 if list is empty (may be indistinguishable from actual 0)
 * @see ilist2_empty()
 */
#define ilist2_get_front(h) \
    ((typeof(*h))(*(typeof(h))ilist2_get_front_fn((ilist2 *)h)))

/**
 * @def ilist2_pop_back(h)
 * @brief Removes and returns the last element from the list
 *
 * @param h Pointer to the list
 * @return  Value of the removed element, or 0 if list is empty
 *
 * @note The removed node is added to the freelist for reuse
 * @warning Returns 0 if list is empty (may be indistinguishable from actual 0)
 *
 * @code
 * struct MyEntry *list = ilist2_create(list, 16);
 * ilist2_put_back(list, 42);
 * int val = ilist2_pop_back(list);  // val == 42
 * @endcode
 */
#define ilist2_pop_back(h) \
    ((typeof(*h))(*(typeof(h))ilist2_pop_back_fn((ilist2 *)h)))

/**
 * @def ilist2_pop_front(h)
 * @brief Removes and returns the first element from the list
 *
 * @param h Pointer to the list
 * @return  Value of the removed element, or 0 if list is empty
 *
 * @note The removed node is added to the freelist for reuse
 * @warning Returns 0 if list is empty (may be indistinguishable from actual 0)
 */
#define ilist2_pop_front(h) \
    ((typeof(*h))(*(typeof(h))ilist2_pop_front_fn((ilist2 *)h)))

/**
 * @def ilist2_put_back(h, node)
 * @brief Inserts an element at the back of the list
 *
 * @param h    Pointer to the list
 * @param node Value to insert (will be copied by value)
 * @return     Index of the newly inserted node, or ILIST2_UNDEF if list is full
 *
 * @note If list is full, returns ILIST2_UNDEF
 *
 * @code
 * struct MyEntry *list = ilist2_create(list, 16);
 * ilist2_idx_t idx = ilist2_put_back(list, 42);
 * @endcode
 */
#define ilist2_put_back(h, node) \
    ({ \
        ilist2_idx_t idx; \
        typeof(h) e = (typeof(h))ilist2_touch_back_fn((ilist2 *)h, &idx); \
        if (e) { \
            *e = (node); \
        } \
        idx; \
    })

/**
 * @def ilist2_put_front(h, node)
 * @brief Inserts an element at the front of the list
 *
 * @param h    Pointer to the list
 * @param node Value to insert (will be copied by value)
 * @return     Index of the newly inserted node, or ILIST2_UNDEF if list is full
 *
 * @note If list is full, returns ILIST2_UNDEF
 */
#define ilist2_put_front(h, node) \
    ({ \
        ilist2_idx_t idx; \
        typeof(h) e = (typeof(h))ilist2_touch_front_fn((ilist2 *)h, &idx); \
        if (e) { \
            *e = (node); \
        } \
        idx; \
    })

/**
 * @def ilist2_move_back_by_idx(h, idx)
 * @brief Moves an existing node to the back of the list
 *
 * @param h   Pointer to the list
 * @param idx Index of the node to move
 *
 * @pre idx must be a valid index previously returned by ilist2_put_back() or ilist2_put_front()
 *
 * @note If idx is already at the back, does nothing
 * @warning Does NOT check if idx is valid in release builds
 *
 * @code
 * struct MyEntry *list = ilist2_create(list, 16);
 * ilist2_idx_t idx = ilist2_put_back(list, 42);
 * ilist2_put_back(list, 100);
 * ilist2_move_back_by_idx(list, idx);  // 42 moves to back
 * @endcode
 */
#define ilist2_move_back_by_idx(h, idx) \
    ilist2_move_back_by_idx_fn((ilist2 *)h, idx)

/**
 * @def ilist2_move_front_by_idx(h, idx)
 * @brief Moves an existing node to the front of the list
 *
 * @param h   Pointer to the list
 * @param idx Index of the node to move
 *
 * @pre idx must be a valid index previously returned by ilist2_put_back() or ilist2_put_front()
 *
 * @note If idx is already at the front, does nothing
 * @warning Does NOT check if idx is valid in release builds
 */
#define ilist2_move_front_by_idx(h, idx) \
    ilist2_move_front_by_idx_fn((ilist2 *)h, idx)

/**
 * @def ilist2_empty(h)
 * @brief Checks if the list is empty
 *
 * @param h Pointer to the list
 * @return  1 if list is empty, 0 otherwise
 */
#define ilist2_empty(h) \
    (((ilist2 *)(h))->front_idx == ILIST2_UNDEF)

/**
 * @def ilist2_get_required_memory_size(listsz, usersz)
 * @brief Utility macro to calculate the number of bytes required for list
 *
 * @param listsz  Number of slots in the list
 * @param usersz  Size of each user's entry in bytes
 * @return        Number of bytes required
 *
 * @code
 * struct MyEntry {
 *     int value;
 * };
 * size_t size = ilist2_get_required_memory_size(16, sizeof(MyEntry));
 * struct MyEntry *list = malloc(size);
 * list = ilist2_init(list, 16);
 * @endcode
 */
#define ilist2_get_required_memory_size(listsz, usersz) \
    (sizeof(ilist2) + (usersz + sizeof(ilist2_idx_t) + sizeof(ilist2_idx_t)) * listsz)


ilist2 *ilist2_create_fn(size_t listsz, size_t usersz);
ilist2 *ilist2_init_fn(void *p, size_t listsz, size_t usersz);
void *ilist2_get_back_fn(ilist2 *list);
void *ilist2_get_front_fn(ilist2 *list);
void *ilist2_pop_back_fn(ilist2 *list);
void *ilist2_pop_front_fn(ilist2 *list);
void *ilist2_touch_back_fn(ilist2 *list, ilist2_idx_t *idx);
void ilist2_move_back_by_idx_fn(ilist2 *list, ilist2_idx_t idx);
void ilist2_move_front_by_idx_fn(ilist2 *list, ilist2_idx_t idx);
void *ilist2_touch_front_fn(ilist2 *list, ilist2_idx_t *idx);

#ifndef NDEBUG

void ilist2_dump_list(void *h);
void ilist2_dump_idx(void *h);
void ilist2_dump_freelist(void *h);

#else

#define ilist2_dump_list(h)    ((void)0)
#define ilist2_dump_idx(h)    ((void)0)
#define ilist2_dump_freelist(h)  ((void)0)

#endif /* NDEBUG */

#endif /* _ILIST2_H_ */

