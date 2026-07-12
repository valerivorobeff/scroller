#ifndef _MEMORY_H_
#define _MEMORY_H_

#include <stddef.h>
#ifndef NDEBUG
    #include <stdint.h>
#endif

typedef struct Context Context;

extern Context *g_context;

extern size_t MEMORY_PAGESZ;

/**
 * @brief Default memory size (1 MB)
 */
#define CONTEXT_DEFAULTSZ (get_memory_page_size() * 256)

/**
 * @code
 *
 * memory_init(bump_context_create(get_memory_page_size()));
 *
 * memory_drop();
 *
 * @endcode
 */
void memory_init(Context *context); /** This function should be called before any other */
#define memory_init_default() memory_init(bump_context_create(CONTEXT_DEFAULTSZ))
void memory_destroy(void);
size_t get_memory_page_size(void);

typedef void *(*alloc_fn)(Context *context, size_t size);
typedef void *(*realloc_fn)(Context *context, void *p, size_t size);
typedef void (*free_fn)(Context *context, void *p);
typedef void (*reset_fn)(Context *context);
typedef int (*drop_fn)(Context *context);
typedef Context *(*add_child_fn)(Context *context, Context *child);
typedef int (*erase_child_fn)(Context *context, Context *child);
typedef void (*set_parent_fn)(Context *context, Context *parent);
typedef Context *(*get_parent_fn)(Context *context);

#define CONTEXT_MAX_CHILDREN 8

typedef struct ContextVt {
    alloc_fn alloc;
    realloc_fn realloc;
    free_fn free;
    reset_fn reset;
    drop_fn drop;
    add_child_fn add_child;
    erase_child_fn _erase_child;
    set_parent_fn _set_parent;
    get_parent_fn _get_parent;
} ContextVt;

typedef struct Context {
    const ContextVt *vt; /* it should be the first in struct */
} Context;

typedef struct BumpContext {
    const ContextVt *vt; /* it should be the first in struct */
#ifndef NDEBUG
    uint32_t magic;
#endif
    size_t size;
    size_t current;
    Context *parent;
    Context *children[CONTEXT_MAX_CHILDREN];
} BumpContext;

Context *bump_context_create(size_t size);
Context *linear_context_create(size_t size);

static inline Context *context_get_current(void) {
    return g_context;
}

static inline Context *context_switch(Context *context) {
    Context *old_context = g_context;

    g_context = context;

    return old_context;
}

#define context_alloc(context, sz) ((Context *)(context))->vt->alloc(context, sz)
#define context_realloc(context, p, sz) ((Context *)(context))->vt->realloc(context, p, sz)
#define context_free(context, p) ((Context *)(context))->vt->free(context, p)
#define context_reset(context) ((Context *)(context))->vt->reset(context)
#define context_drop(context) ((Context *)(context))->vt->drop(context)
#define context_add_child(context, child) ((Context *)(context))->vt->add_child(context, child)

#define salloc(sz) context_alloc(g_context, sz)
#define srealloc(p, sz) context_realloc(g_context, p, sz)
#define sfree(p) context_free(g_context, p)

#endif /* _MEMORY_H_ */

