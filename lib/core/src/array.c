#include "array.h"

/**
 * @note: if you use memory library (memory.h) intialize it first
 * @code:
 * memory_init_default();
 * int *a = array_create(a, 16);
 * array_put(a, 5);
 * array_free(a);
 * memory_destroy();
 * @endcode
 */ 

#ifndef my_alloc
#include "memory.h"
#define memory_included
#define my_alloc salloc
#endif

#ifndef my_realloc
#ifndef memory_included
#include "memory.h"
#endif
#define my_realloc srealloc
#endif

#ifndef my_free
#ifndef memory_included
#include "memory.h"
#endif
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
    return get_header(a)->size;
}

void *
array_raise_fn(void *a) {
    Array *header = get_header(a);

    if (header->size == header->capacity) {
        header->capacity *= 2;
        header = my_realloc(header, sizeof(Array) + header->usersz * header->capacity);
        a = (char *)header + sizeof(Array);
    }

    ++header->size;

    return a;
}

void *
array_reduce_fn(void *a) {
    Array *header = get_header(a);

    --header->size;

    if (header->size) {
        if (header->size == header->capacity / 2) {
            header->capacity /= 2;
            if (header->capacity) {
                header = my_realloc(header, sizeof(Array) + header->usersz * header->capacity);
                a = (char *)header + sizeof(Array);
            }
        }
    } else
        my_free(a);

    return a;
}

