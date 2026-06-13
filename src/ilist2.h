#ifndef _ILIST2_H_
#define _ILIST2_H_

#include <stddef.h>
#include <sys/types.h>

#define ILIST2_UNDEF (ssize_t)-1

typedef ssize_t ilist2_idx_t;

typedef struct ilist2 {
    size_t listsz;
    ilist2_idx_t freelist_head;
    ilist2_idx_t front_idx;
    ilist2_idx_t back_idx;
    size_t nodesz;
    char nodes[];
} ilist2;

#define ilist2_create(h, listsz_) \
    (typeof(h))ilist2_create_fn(listsz_, sizeof(*h))

#define ilist2_init(p, h, listsz_) \
    (typeof(h))ilist2_init_fn( p, listsz_, sizeof(*h))

void ilist2_clear(void *p);

void ilist2_free(void *p);

#define ilist2_get_back(h) \
    ((typeof(*h))(*(typeof(h))ilist2_get_back_fn((ilist2 *)h)))

#define ilist2_get_front(h) \
    ((typeof(*h))(*(typeof(h))ilist2_get_front_fn((ilist2 *)h)))

#define ilist2_pop_back(h) \
    ((typeof(*h))(*(typeof(h))ilist2_pop_back_fn((ilist2 *)h)))

#define ilist2_pop_front(h) \
    ((typeof(*h))(*(typeof(h))ilist2_pop_front_fn((ilist2 *)h)))

#define ilist2_put_back(h, node) \
    ({ \
        typeof(h) e = (typeof(h))ilist2_touch_back_fn((ilist2 *)h); \
        if (e) { \
            *e = (node); \
        } \
        e; \
    })

#define ilist2_put_front(h, node) \
    ({ \
        typeof(h) e = (typeof(h))ilist2_touch_front_fn((ilist2 *)h); \
        if (e) { \
            *e = (node); \
        } \
        e; \
    })

#define ilist2_empty(h) \
    (((ilist2 *)(h))->front_idx == ILIST2_UNDEF)

#define ilist2_get_required_memory_size(listsz, usersz) \
    (sizeof(ilist2) + (usersz + sizeof(ilist2_idx_t) + sizeof(ilist2_idx_t)) * listsz)


ilist2 *ilist2_create_fn(size_t listsz, size_t usersz);
ilist2 *ilist2_init_fn(void *p, size_t listsz, size_t usersz);
void *ilist2_get_back_fn(ilist2 *list);
void *ilist2_get_front_fn(ilist2 *list);
void *ilist2_pop_back_fn(ilist2 *list);
void *ilist2_pop_front_fn(ilist2 *list);
void *ilist2_touch_back_fn(ilist2 *list);
void *ilist2_touch_front_fn(ilist2 *list);

#ifndef NDEBUG

void ilist2_dump_simple(void *h);
void ilist2_dump_debug(void *h);
void ilist2_dump_freelist(void *h);

#else

#define ilist2_dump_simple(h)    ((void)0)
#define ilist2_dump_debug(h)     ((void)0)
#define ilist2_dump_freelist(h)  ((void)0)

#endif /* NDEBUG */

#endif /* _ILIST2_H_ */

