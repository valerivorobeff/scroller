/**
 * @file memory.c
 * @brief Memory context manager implementation
 */

#include "memory.h"
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>

#define CONTEXT_STACK_MAX   16
static Context *context_stack[CONTEXT_STACK_MAX];
static int context_stack_i = 0;

/** @brief Root context for cleanup */
static Context *g_root_context = NULL;

/** @brief Global current context */
Context *g_context = NULL;

/** @brief System page size in bytes */
size_t MEMORY_PAGESZ = 4096;

#ifndef NDEBUG
/** @brief Magic number for debug verification */
static const uint32_t magic = 0xFEEDFACE;
#endif

/* Forward declarations */
void memory_init(Context *context);
void memory_destroy(void);
size_t get_memory_page_size(void);
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

/** @brief Maximum alignment for allocation */
#define MAX_ALIGN _Alignof(max_align_t)

void context_push(Context *context);
void context_pop();
char *sdup(const char *src);
static inline size_t align_up(size_t sz, int align);
static inline size_t align_max(size_t sz);

/*
 * Memory init
 */

/**
 * @brief Initialize memory system with root context
 * @param context Root context
 */
void
memory_init(Context *context) {
    get_memory_page_size();
    g_root_context = g_context = context;
}

/**
 * @brief Destroy memory system and free all contexts
 */
void
memory_destroy(void) {
    if (g_root_context) {
        context_drop(g_root_context);
        g_root_context = NULL;
    }
    g_context = NULL;
}

/**
 * @brief Get system page size (cached after first call)
 * @return Page size in bytes
 */
size_t
get_memory_page_size(void) {
    static int initialized = 0;

    if (!initialized) {
        MEMORY_PAGESZ = sysconf(_SC_PAGESIZE);
        initialized = 1;
    }

    return MEMORY_PAGESZ;
}

/*
 * BumpContext
 */

/** @brief Virtual table for bump context */
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

/**
 * @brief Create bump allocator context
 * @param size Context size in bytes
 * @return New context, or NULL on failure
 */
Context *
bump_context_create(size_t size) {
    BumpContext *ret;

    if (size == 0)
        return NULL;

    ret = mmap(NULL, size, PROT_READ | PROT_WRITE,
               MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    /* @todo: handle errno */
    if (ret == MAP_FAILED) {
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

/**
 * @brief Reset bump context (free all allocations)
 * @param context Context to reset
 */
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

/**
 * @brief Drop bump context (free memory and children)
 * @param context Context to drop
 * @return 0 on success, non-zero on error
 */
int
bump_context_drop(Context *context) {
    BumpContext *bcontext = (BumpContext *)context;

    if (context == NULL)
        return 0;

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

/**
 * @brief Add child to bump context
 * @param context Parent context
 * @param child Child context to add
 * @return Added child on success, NULL on failure
 */
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

/**
 * @brief Allocate from bump context
 * @param context Context to allocate from
 * @param sz Number of bytes to allocate
 * @return Allocated memory, or NULL on failure
 */
void *
bump_context_alloc(Context *context, size_t sz) {
    BumpContext *bcontext = (BumpContext *)context;
    const size_t sizesz = align_max(sizeof(size_t));
    size_t totalsz;

    if (sz == 0)
        return NULL;

#ifndef NDEBUG
    assert(bcontext->magic == magic);
#endif

    sz = align_max(sz);
    totalsz = sz + sizesz; /* Data size + size size */

    assert(sz <= SIZE_MAX - sizesz);

    if (bcontext->size - bcontext->current < totalsz)
        return NULL;
    else {
        void *ret = (char *)context + bcontext->current;
        *(size_t *)ret = sz;            /* Write down the block size before data */
        bcontext->current += totalsz;   /* Increment current by total size */
        return (char *)ret + sizesz;    /* Return pointer to data */
    }
}

/**
 * @brief Reallocate from bump context
 * @param context Context to reallocate from
 * @param p Previously allocated pointer
 * @param sz New size in bytes
 * @return Reallocated memory, or NULL on failure
 */
void *
bump_context_realloc(Context *context, void *p, size_t sz) {
    if (p == NULL)
        return bump_context_alloc(context, sz);

    if (sz == 0) {
        bump_context_free(context, p);
        return NULL;
    }

    const size_t sizesz = align_max(sizeof(size_t));
    BumpContext *bcontext = (BumpContext *)context;
    const size_t psz = *(size_t *)((char *)p - sizesz); /* Previous size */
    assert(psz > 0 && psz <= bcontext->size);

#ifndef NDEBUG
    if ((char *)p < (char *)context || 
        (char *)p >= (char *)context + bcontext->current) {
        assert(0 && "Invalid pointer in free");
    }
    assert(bcontext->magic == magic);
#endif

    sz = align_max(sz);
    assert(sz <= SIZE_MAX - sizesz);

    if (sz > psz) {
        void *np;
        const size_t p_offs = (char *)p - (char *)context;
        /* If p is the tail, rewind bcontext->current to it */
        if (p_offs == bcontext->current - sizesz)
            bcontext->current = p_offs - sizesz;

        np = bump_context_alloc(context, sz);
        if (np && np != p)
            memcpy(np, p, psz);

        return np;
    } else
        return p;
}

/**
 * @brief Free from bump context (only if pointer is tail)
 * @param context Context to free from
 * @param p Pointer to free
 */
void
bump_context_free(Context *context, void *p) {
    if (p == NULL)
        return;

    const size_t sizesz = align_max(sizeof(size_t));
    BumpContext *bcontext = (BumpContext *)context;
    const size_t p_offs = (char *)p - (char *)context;
    const size_t sz = *(size_t *)((char *)p - sizesz); /* Size */
    assert(sz > 0 && sz <= bcontext->size);


#ifndef NDEBUG
    if ((char *)p < (char *)context || 
        (char *)p >= (char *)context + bcontext->current) {
        assert(0 && "Invalid pointer in free");
    }
    assert(bcontext->magic == magic);
#endif

    /* free if only p is the tail */
    if (p_offs == bcontext->current - sizesz)
        bcontext->current = p_offs - sizesz;
}

/**
 * @brief Erase child from bump context
 * @param context Parent context
 * @param child Child to erase
 * @return 0 on success, 1 if not found
 */
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

/**
 * @brief Set parent for bump context
 * @param context Child context
 * @param parent New parent
 */
void
bump_context_set_parent(Context *context, Context *parent) {
    ((BumpContext *)context)->parent = parent;
}

/**
 * @brief Get parent of bump context
 * @param context Child context
 * @return Parent context, or NULL
 */
Context *
bump_context_get_parent(Context *context) {
    return ((BumpContext *)context)->parent;
}

/*
 * LinearContext
 */

/** @brief Virtual table for linear context */
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

/**
 * @brief Create linear allocator context (no realloc/free support)
 * @param size Context size in bytes
 * @return New context, or NULL on failure
 */
Context *
linear_context_create(size_t size) {
    BumpContext *ret;

    if (size == 0)
        return NULL;

    ret = mmap(NULL, size, PROT_READ | PROT_WRITE,
               MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    /* @todo: handle errno */
    if (ret == MAP_FAILED) {
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

/**
 * @brief Allocate from linear context
 * @param context Context to allocate from
 * @param sz Number of bytes to allocate
 * @return Allocated memory, or NULL on failure
 */
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

/**
 * @brief Reallocate from linear context (NOT SUPPORTED)
 * @param context Context
 * @param p Pointer
 * @param sz New size
 * @return Never returns (asserts)
 */
void *
linear_context_realloc(Context *context, void *p, size_t sz) {
    (void)context;
    (void)p;
    (void)sz;

    assert(0 && "linear_context_realloc not supported");
    return NULL;
}

/**
 * @brief Free from linear context (NOT SUPPORTED)
 * @param context Context
 * @param p Pointer
 */
void
linear_context_free(Context *context, void *p) {
    (void)context;
    (void)p;

    assert(0 && "linear_context_free not supported");
}

void
context_push(Context *context) {
    assert(context_stack_i != CONTEXT_STACK_MAX);

    context_stack[context_stack_i++] = context_get_current();
    g_context = context;
}

void
context_pop() {
    assert(context_stack_i != 0);

    g_context = context_stack[context_stack_i--];
}

char *
sdup(const char *src) {
    const size_t len = strlen(src);
    char *ret = salloc(len + 1);
    memcpy(ret, src, len);
    ret[len] = '\0';

    return ret;
}

/**
 * @brief Align size up to specified alignment
 * @param sz Size to align
 * @param align Alignment (must be power of two)
 * @return Aligned size
 */
static inline size_t align_up(size_t sz, int align) {
    return (sz + align - 1) & ~(align - 1);
}

/**
 * @brief Align size to maximum alignment
 * @param sz Size to align
 * @return Aligned size
 */
static inline size_t align_max(size_t sz) {
    return align_up(sz, MAX_ALIGN);
}

