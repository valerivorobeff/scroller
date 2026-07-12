#include "memory.h"
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include <unistd.h>
Context *g_context = NULL;

size_t MEMORY_PAGESZ;

#ifndef NDEBUG
    static const uint32_t magic = 0xFEEDFACE;
#endif

void memory_init(void);

Context *bump_context_create(size_t size);

static void *bump_context_alloc(Context *context, size_t sz);
static void *bump_context_realloc(Context *context, void *p, size_t sz);
static void bump_context_free(Context *context, void *p);
static void bump_context_reset(Context *context);
static int bump_context_drop(Context *context);
static Context *bump_context_add_child(Context *context, Context *child);
static int bump_context_erase_child(Context *context, Context *child);
static void bump_context_set_parent(Context *context, Context *parent);
static Context *bump_context_get_parent(Context *context);

Context *linear_context_create(size_t size);

static void *linear_context_alloc(Context *context, size_t sz);
static void *linear_context_realloc(Context *context, void *p, size_t sz);
static void linear_context_free(Context *context, void *p);

#define MAX_ALIGN _Alignof(max_align_t)

static inline size_t align_up(size_t sz, int align);
static inline size_t align_max(size_t sz);

/*
 * Memory init
 */

void memory_init(void) {
    MEMORY_PAGESZ = sysconf(_SC_PAGESIZE);
}

/*
 * LinearContext
 */

static const ContextVt bump_context_vt = {
    bump_context_alloc,
    bump_context_realloc,
    bump_context_free,
    bump_context_reset,
    bump_context_drop,
    bump_context_add_child,
    bump_context_erase_child,
    bump_context_set_parent,
    bump_context_get_parent
};

Context *
bump_context_create(size_t size) {
    BumpContext *ret = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    if (ret == NULL) {
        assert(ret);
        return NULL;
    }

    ret->vt = &bump_context_vt;
#ifndef NDEBUG
    ret->magic = magic;
#endif
    ret->size = size;
    ret->parent = NULL;
    ret->current = align_max(sizeof(BumpContext));
    for (int i = 0; i != CONTEXT_MAX_CHILDREN; ++i)
        ret->children[i] = NULL;

    return (Context *)ret;
}

void
bump_context_reset(Context *context) {
    BumpContext *bcontext = (BumpContext *)context;

#ifndef NDEBUG
    assert(bcontext->magic == magic);
#endif

    bcontext->current = align_max(sizeof(BumpContext));

    /* Drop children */
    for (int i = 0; i != CONTEXT_MAX_CHILDREN; ++i) {
        Context *child = bcontext->children[i];
        if (child) {
            child->vt->drop(child);
            bcontext->children[i] = NULL;
        }
    }
}

int
bump_context_drop(Context *context) {
    BumpContext *bcontext = (BumpContext *)context;

#ifndef NDEBUG
    assert(bcontext->magic == magic);
#endif

    /* Drop children */
    for (int i = 0; i != CONTEXT_MAX_CHILDREN; ++i) {
        Context *child = bcontext->children[i];
        if (child)
           child->vt->drop(child);
    }

    if (bcontext->parent)
        bcontext->parent->vt->_erase_child(bcontext->parent, context);

    return munmap(bcontext, bcontext->size);
}

Context *
bump_context_add_child(Context *context, Context *child) {
    BumpContext *bcontext = (BumpContext *)context;

    for (int i = 0; i != CONTEXT_MAX_CHILDREN; ++i) {
        if (bcontext->children[i] == NULL) {
            bcontext->children[i] = child;

            child->vt->_set_parent(child, context);

            return child;
        }
    }

    return NULL;
}

void *
bump_context_alloc(Context *context, size_t sz) {
    BumpContext *bcontext = (BumpContext *)context;
    size_t sizesz = align_max(sizeof(size_t));
    size_t totalsz;

    if (sz == 0)
        return NULL;

#ifndef NDEBUG
    assert(bcontext->magic == magic);
#endif

    sz = align_max(sz);
    totalsz = sz + sizesz;

    assert(sz <= SIZE_MAX - sizesz);

    if (bcontext->size - bcontext->current < totalsz)
        return NULL;
    else {
        void *ret = (char *)context + bcontext->current;
        *(size_t *)ret = sz;
        bcontext->current += totalsz;

        return (char *)ret + sizesz;
    }
}

void *
bump_context_realloc(Context *context, void *p, size_t sz) {
    size_t sizesz = align_max(sizeof(size_t));
    size_t totalsz;

    if (sz == 0) {
        bump_context_free(context, p);
        return NULL;
    }

    if (p == NULL)
        return bump_context_alloc(context, sz);

#ifndef NDEBUG
    if ((char *)p < (char *)context || 
        (char *)p >= (char *)context + ((BumpContext *)context)->current) {
        assert(0 && "Invalid pointer in free");
    }

    assert(((BumpContext *)context)->magic == magic);
#endif

    const size_t psz = *(size_t *)((char *)p - sizesz);
    assert(psz > 0 && psz <= ((BumpContext *)context)->size);

    sz = align_max(sz);
    totalsz = sz + sizesz;

    assert(sz <= SIZE_MAX - sizesz);

    if (sz > psz) {
        void *np = bump_context_alloc(context, sz);
        memcpy(np, p, psz);

        return np;
    } else
        return p;
}

void
bump_context_free(Context *context, void *p) {
    if (p == NULL)
        return;

    BumpContext *bcontext = (BumpContext *)context;

#ifndef NDEBUG
    if ((char *)p < (char *)context || 
        (char *)p >= (char *)context + bcontext->current) {
        assert(0 && "Invalid pointer in free");
    }

    assert(bcontext->magic == magic);
#endif

    const size_t sz = *(size_t *)((char *)p - align_max(sizeof(size_t)));
    assert(sz > 0 && sz <= bcontext->size);

    /* free if only p is the tail */
    if ((char *)p - (char *)context + sz >= bcontext->current)
        bcontext->current = (char *)p - (char *)context;
}

int
bump_context_erase_child(Context *context, Context *child) {
    BumpContext *bcontext = (BumpContext *)context;
    
    for (int i = 0; i != CONTEXT_MAX_CHILDREN; ++i) {
        if (bcontext->children[i] == child) {
            bcontext->children[i] = NULL;
            return 0;
        }
    }

    return 1;
}

void
bump_context_set_parent(Context *context, Context *parent) {
    ((BumpContext *)context)->parent = parent;
}

Context *
bump_context_get_parent(Context *context) {
    return ((BumpContext *)context)->parent;
}

/*
 * LinearContext
 */

static const ContextVt linear_context_vt = {
    linear_context_alloc,
    linear_context_realloc,
    linear_context_free,
    bump_context_reset,
    bump_context_drop,
    bump_context_add_child,
    bump_context_erase_child,
    bump_context_set_parent,
    bump_context_get_parent
};

Context *
linear_context_create(size_t size) {
    BumpContext *ret = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    if (ret == NULL) {
        assert(ret);
        return NULL;
    }

    ret->vt = &linear_context_vt;
#ifndef NDEBUG
    ret->magic = magic;
#endif
    ret->size = size;
    ret->parent = NULL;
    ret->current = align_max(sizeof(BumpContext));
    for (int i = 0; i != CONTEXT_MAX_CHILDREN; ++i)
        ret->children[i] = NULL;

    return (Context *)ret;
}

void *
linear_context_alloc(Context *context, size_t sz) {
    BumpContext *bcontext = (BumpContext *)context;

#ifndef NDEBUG
    assert(bcontext->magic == magic);
#endif

    if (sz == 0)
        return NULL;

    sz = align_max(sz);

    assert(sz <= SIZE_MAX);

    if (bcontext->size - bcontext->current < sz)
        return NULL;
    else {
        void *ret = (char *)context + bcontext->current;
        bcontext->current += sz;

        return ret;
    }
}

void *
linear_context_realloc(Context *context, void *p, size_t sz) {
    (void)context;
    (void)p;
    (void)sz;

    assert(0 && "linear_context_realloc not supported");

    return NULL;
}

void
linear_context_free(Context *context, void *p) {
    (void)context;
    (void)p;

    assert(0 && "linear_context_free not supported");
}

static inline size_t align_up(size_t sz, int align) {
    return (sz + align - 1) & ~(align - 1);
}

static inline size_t align_max(size_t sz) {
    return align_up(sz, MAX_ALIGN);
}

#include <unistd.h>
#include <stdio.h>

int main() {
    memory_init();

    g_context = bump_context_create(MEMORY_PAGESZ);
    if (!g_context) {
        fprintf(stderr, "Failed to create main context\n");
        return 1;
    }

    Context *child_context = linear_context_create(MEMORY_PAGESZ * 2);
    if (!child_context) {
        fprintf(stderr, "Failed to create child context\n");
        context_drop(g_context);
        return 1;
    }

    context_add_child(g_context, child_context);

    int *i = salloc(sizeof(int));

    *i = 5;

    i = srealloc(i, sizeof(int) * 3);

    i[1] = 7;

    i[2] = 9;

    sfree(i);

    context_drop(g_context);

    return 0;
}

