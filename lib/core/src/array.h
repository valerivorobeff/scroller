#ifndef _ARRAY_H_
#define _ARRAY_H_

#include <stddef.h>

#define array_create(a, capacity) \
    (typeof(a))array_create_fn(sizeof(*a), capacity)

void array_free(void *a);
size_t array_size(void *a);

#define array_back(a) \
    (a)[array_size(a) - 1]

#define array_put(a, v) \
    ({ \
        a = (typeof(a))array_raise_fn(a); \
        array_back(a) = v; \
        a; \
    })

#define array_pop(a) \
    ({ \
        typeof(a) ret = array_back(a); \
        a = (typeof(a))array_reduce_fn(a); \
        ret; \
    })

void *array_create_fn(size_t usersz, size_t capacity);
void *array_raise_fn(void *a);
void *array_reduce_fn(void *a);

#endif /* _ARRAY_H_ */

