#include "ihash.h"
#include <malloc.h>
#include <string.h>

/**
 * Static functions and data
 */

#define ihash_get_tab(h)   ((void *)((char *)h + sizeof(ihash)))
#define ihash_get_nodes(h) (ihash_get_tab(h) + sizeof(h) * ((ihash*)(h))->size)
static ssize_t hash_fn(ssize_t key);

/**
 * Public functions
 */

void ihash_free(void *hash);
ihash *ihash_create_fn(size_t size, size_t cap,
    size_t keyoffs, size_t nextoffs, size_t entrysz);
void *ihash_get_fn(ihash *hash, ssize_t key, size_t keyoffs, size_t nextoffs, size_t entrysz);
void *ihash_put_fn(ihash *hash, ssize_t key, size_t keyoffs, size_t nextoffs, size_t valueoffs, size_t valuesz, void *value, size_t entrysz);

/**
 * Implementation
 */

static ssize_t
hash_fn(ssize_t key) {
    return key;
}

void
ihash_free(void *hash) {
    free((ihash *)hash);
}

ihash *
ihash_create_fn(size_t size, size_t cap,
    size_t keyoffs, size_t nextoffs, size_t entrysz) {
    void *e;

    ihash *hash = malloc(sizeof(ihash) + entrysz * (size + cap));
    hash->size = size;
    hash->cap = cap;
    hash->head_node = 0;
    e = ihash_get_tab(hash);

    for (size_t i = 0; i != size; ++i, e += entrysz)
        *(ssize_t *)(e + keyoffs) = *(ssize_t *)(e + nextoffs) = IHASH_UNDEF;

    for (size_t i = 0; i != cap; ++i, e += entrysz) {
        *(ssize_t *)(e + keyoffs) = IHASH_UNDEF;
        *(ssize_t *)(e + nextoffs) = i + 1;
    }

    *(ssize_t *)(e - entrysz + nextoffs) = IHASH_UNDEF;

    return hash;
}

void *
ihash_get_fn(ihash *hash, ssize_t key, size_t keyoffs, size_t nextoffs, size_t entrysz) {
    size_t idx = hash_fn(key) % hash->size;          /* hash sum */
    void *e = ihash_get_tab(hash) + idx * entrysz;  /* hash entry */
    void *nodes = ihash_get_nodes(hash);            /* nodes tab */

    if (IHASH_UNDEF == *(ssize_t *)(e + keyoffs))
        return NULL;

    for(;;) {
        if (key == *(ssize_t *)(e + keyoffs))
            return e;

        if (IHASH_UNDEF == *(ssize_t *)(e + nextoffs))
            return NULL;

        e = nodes + *(ssize_t *)(e + nextoffs) * entrysz;
    }
}

void *
ihash_put_fn(ihash *hash, ssize_t key, size_t keyoffs, size_t nextoffs, size_t valueoffs, size_t valuesz, void *value, size_t entrysz) {
    size_t idx = hash_fn(key) % hash->size;         /* hash sum */
    void *e = ihash_get_tab(hash) + idx * entrysz;  /* hash entry */
    void *nodes = ihash_get_nodes(hash);            /* nodes tab */

    if ((size_t)IHASH_UNDEF == *(size_t *)(e + keyoffs)) {
        *(size_t *)(e + keyoffs) = key;
        *(size_t *)(e + nextoffs) = IHASH_UNDEF;
        memcpy(e + valueoffs, value, valuesz);
        return e;
    }

    for(;;) {
        if (key == *(ssize_t *)(e + keyoffs))
            return e;

        if (IHASH_UNDEF == *(ssize_t *)(e + nextoffs)) {
            if (hash->head_node == IHASH_UNDEF)
                return NULL;

            void *phead_node = nodes + hash->head_node * entrysz;
            *(ssize_t *)(e + nextoffs) = hash->head_node;
            hash->head_node = *(ssize_t *)(phead_node + nextoffs);
            return nodes + *(ssize_t *)(e + nextoffs) * entrysz;
        }

        e = nodes + *(ssize_t *)(e + nextoffs) * entrysz;
    }
}

