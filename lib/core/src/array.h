#ifndef _ARRAY_H_
#define _ARRAY_H_

#include <stddef.h>

/** @brief Array default capacity */
#define ARRAY_DEFAULT_CAPACITY 128

/**
 * @brief Creates a new dynamic array
 * @param a        Pointer to variable that will receive the array
 * @param capacity Initial capacity (number of elements)
 * @return         Pointer to created array (cast to type of a)
 */
#define array_create(a, capacity) \
    (typeof(a))array_create_fn(sizeof(*a), capacity)

/**
 * @brief Frees a dynamic array
 * @param a Pointer to array to free
 */
void array_free(void *a);

/**
 * @brief Clears the array, doesn't change its capacity
 * @param a Pointer to array to free
 */
/** @todo: make unitest for array_clear */
void array_clear(void *a);

/**
 * @brief Returns the number of elements in the array
 * @param a Pointer to array
 * @return  Number of elements
 */
size_t array_size(const void *a);

/**
 * @brief Checks if the array is empty
 * @param a Pointer to array
 * @return  1 if empty, 0 otherwise
 */
#define array_empty(a) \
    (array_size(a) == 0)

/**
 * @brief Returns a copy of the last element
 * @param a Pointer to array
 * @return  Copy of last element
 * @warning Undefined behavior if array is empty
 */
#define array_back(a) \
    ({ \
        typeof(*(a)) _ret = (a)[array_size(a) - 1]; \
        _ret; \
    })

/**
 * @brief Returns a reference to the last element (for writing)
 * @param a Pointer to array
 * @return  Reference to last element
 * @warning Undefined behavior if array is empty
 */
#define array_back_ref(a) \
    ((a)[array_size(a) - 1])

/**
 * @brief Appends an element to the end of the array
 * @param a Pointer to array (will be updated if reallocation occurs)
 * @param v Value to append
 * @note If reallocation fails, returns NULL
 */
#define array_put(a, v) \
    ({ \
        typeof(a) _new; \
        if (!a) \
            a = array_create(a, ARRAY_DEFAULT_CAPACITY); \
        _new = (typeof(a))array_raise_fn(a); \
        if (_new) { \
            a = _new; \
            array_back_ref(a) = v; \
        } \
    })

/**
 * @brief Removes and returns the last element
 * @param a Pointer to array (will be updated or set to NULL if empty)
 * @return  Removed element
 * @warning Undefined behavior if array is empty
 */
#define array_pop(a) \
    ({ \
        typeof(*(a)) _ret = array_back(a); \
        a = (typeof(a))array_reduce_fn(a); \
        _ret; \
    })

/**
 * @brief Internal functions
 */
void *array_create_fn(size_t usersz, size_t capacity);
void *array_raise_fn(void *a);
void *array_reduce_fn(void *a);

#endif /* _ARRAY_H_ */

