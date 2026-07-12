/**
 * @file memory.h
 * @brief Memory context manager with bump and linear allocators
 */

#ifndef _MEMORY_H_
#define _MEMORY_H_

#include <stddef.h>
#ifndef NDEBUG
    #include <stdint.h>
#endif

/**
 * @brief Opaque memory context type
 */
typedef struct Context Context;

/** @brief Global current context for salloc/srealloc/sfree */
extern Context *g_context;

/** @brief System page size in bytes */
extern size_t MEMORY_PAGESZ;

/**
 * @brief Default memory context size (1 MB)
 */
#define CONTEXT_DEFAULTSZ (get_memory_page_size() * 256)

/**
 * @brief Initialize memory system with root context
 * @param context Root context (usually bump_context_create)
 *
 * @code
 * memory_init(bump_context_create(get_memory_page_size()));
 * @endcode
 */
void memory_init(Context *context);

/**
 * @brief Initialize with default-sized bump context
 */
#define memory_init_default() memory_init(bump_context_create(CONTEXT_DEFAULTSZ))

/**
 * @brief Destroy memory system and free all contexts
 */
void memory_destroy(void);

/**
 * @brief Get system page size (cached after first call)
 * @return Page size in bytes
 */
size_t get_memory_page_size(void);

/**
 * @brief Allocation function type
 * @param context Memory context
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL on failure
 */
typedef void *(*alloc_fn)(Context *context, size_t size);

/**
 * @brief Reallocation function type
 * @param context Memory context
 * @param p Previously allocated pointer, or NULL
 * @param size New size in bytes
 * @return Pointer to reallocated memory, or NULL on failure
 */
typedef void *(*realloc_fn)(Context *context, void *p, size_t size);

/**
 * @brief Free function type
 * @param context Memory context
 * @param p Pointer to free
 */
typedef void (*free_fn)(Context *context, void *p);

/**
 * @brief Reset function type (clear all allocations)
 * @param context Memory context
 */
typedef void (*reset_fn)(Context *context);

/**
 * @brief Drop function type (free context and all children)
 * @param context Memory context
 * @return 0 on success, non-zero on error
 */
typedef int (*drop_fn)(Context *context);

/**
 * @brief Add child context function type
 * @param context Parent context
 * @param child Child context to add
 * @return Added child on success, NULL on failure
 */
typedef Context *(*add_child_fn)(Context *context, Context *child);

/**
 * @brief Erase child context function type
 * @param context Parent context
 * @param child Child context to remove
 * @return 0 on success, 1 if child not found
 */
typedef int (*erase_child_fn)(Context *context, Context *child);

/**
 * @brief Set parent context function type
 * @param context Child context
 * @param parent New parent context
 */
typedef void (*set_parent_fn)(Context *context, Context *parent);

/**
 * @brief Get parent context function type
 * @param context Child context
 * @return Parent context, or NULL if none
 */
typedef Context *(*get_parent_fn)(Context *context);

/** @brief Maximum number of children per context */
#define CONTEXT_MAX_CHILDREN 8

/**
 * @brief Virtual table for context operations
 */
typedef struct ContextVt {
    alloc_fn alloc;             /**< Allocate memory */
    realloc_fn realloc;         /**< Reallocate memory */
    free_fn free;               /**< Free memory */
    reset_fn reset;             /**< Reset context */
    drop_fn drop;               /**< Drop context */
    add_child_fn add_child;     /**< Add child context */
    erase_child_fn _erase_child;/**< Internal: erase child */
    set_parent_fn _set_parent;  /**< Internal: set parent */
    get_parent_fn _get_parent;  /**< Internal: get parent */
} ContextVt;

/**
 * @brief Base context structure
 * @note vt must be the first member
 */
typedef struct Context {
    const ContextVt *vt;        /**< Virtual table
                                 * It should preside all the other fields in
                                 * struct 
                                 */
} Context;

/**
 * @brief Bump context structure
 * @note vt must be the first member
 */
typedef struct BumpContext {
    const ContextVt *vt;        /**< Virtual table, should be the first field */
#ifndef NDEBUG
    uint32_t magic;             /**< Magic number for debug checks */
#endif
    size_t size;                /**< Total context size */
    size_t current;             /**< Current allocation offset */
    Context *parent;            /**< Parent context, or NULL */
    Context *children[CONTEXT_MAX_CHILDREN]; /**< Child contexts */
} BumpContext;

/**
 * @brief Create bump allocator context
 * @param size Context size in bytes
 * @return New context, or NULL on failure
 */
Context *bump_context_create(size_t size);

/**
 * @brief Create linear allocator context (no realloc/free support)
 * @param size Context size in bytes
 * @return New context, or NULL on failure
 */
Context *linear_context_create(size_t size);

/**
 * @brief Get current global context
 * @return Current context pointer
 */
static inline Context *context_get_current(void) {
    return g_context;
}

/**
 * @brief Switch global context
 * @param context New context to set
 * @return Previous context
 */
static inline Context *context_switch(Context *context) {
    Context *old_context = g_context;
    g_context = context;
    return old_context;
}

/**
 * @brief Allocate from context
 */
#define context_alloc(context, sz) ((Context *)(context))->vt->alloc(context, sz)

/**
 * @brief Reallocate from context
 */
#define context_realloc(context, p, sz) ((Context *)(context))->vt->realloc(context, p, sz)

/**
 * @brief Free from context
 */
#define context_free(context, p) ((Context *)(context))->vt->free(context, p)

/**
 * @brief Reset context
 */
#define context_reset(context) ((Context *)(context))->vt->reset(context)

/**
 * @brief Drop context
 */
#define context_drop(context) ((Context *)(context))->vt->drop(context)

/**
 * @brief Add child to context
 */
#define context_add_child(context, child) ((Context *)(context))->vt->add_child(context, child)

/** @brief Allocate from global context */
#define salloc(sz) context_alloc(g_context, sz)

/** @brief Reallocate from global context */
#define srealloc(p, sz) context_realloc(g_context, p, sz)

/** @brief Free from global context */
#define sfree(p) context_free(g_context, p)

#endif /* _MEMORY_H_ */

