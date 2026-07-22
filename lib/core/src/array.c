#include "array.h"
#include <assert.h>

/**
 * @note If you use memory library (memory.h), initialize it first:
 * @code
 * memory_init_default();
 * int *a = array_create(a, 16);
 * array_put(a, 5);
 * array_free(a);
 * memory_destroy();
 * @endcode
 */

#ifndef my_alloc
#include "memory.h"
#define my_alloc salloc
#define my_realloc srealloc
#define my_free sfree
#endif

typedef struct Array {
    size_t size;
    size_t usersz;
    size_t capacity;
} Array;

#define get_header(a) ((Array *)((char *)(a) - sizeof(Array)))

void *
array_create_fn(size_t usersz, size_t capacity) {
    void *ret = my_alloc(sizeof(Array) + usersz * capacity);

    if (!ret)
        return NULL;

    ((Array *)(ret))->size = 0;
    ((Array *)(ret))->usersz = usersz;
    ((Array *)(ret))->capacity = capacity;

    return (char *)ret + sizeof(Array);
}

void
array_free(void *a) {
    if (a)
        my_free(get_header(a));
}

size_t
array_size(void *a) {
    return a ? get_header(a)->size : 0;
}

void *
array_raise_fn(void *a) {
    if (!a) {
        assert(0 && "Array pointer is NULL");
        return NULL;
    }

    Array *header = get_header(a);

    if (header->size == header->capacity) {
        header->capacity = header->capacity ? header->capacity * 2 : 1;
        header = my_realloc(header, sizeof(Array) + header->usersz * header->capacity);

        if (!header)
            return NULL;

        a = (char *)header + sizeof(Array);
    }

    ++header->size;
    return a;
}

void *
array_reduce_fn(void *a) {
    Array *header;

    if (!a) {
        assert(0 && "Array pointer is NULL");
        return NULL;
    }

    header = get_header(a);

    if (header->size == 0)
        return NULL;

    --header->size;

    if (header->size == header->capacity / 2 && header->capacity > 4) {
        header->capacity /= 2;
        header = my_realloc(header, sizeof(Array) + header->usersz * header->capacity);

        if (header)
            a = (char *)header + sizeof(Array);
    }

    return a;
}

