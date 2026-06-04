#ifndef _IHASH_H_
#define _IHASH_H_

#include <stddef.h>
#include <sys/types.h>

/**
 * Public structs functions and macros
 */

#define IHASH_UNDEF (ssize_t)-1

typedef struct ihash {
    size_t size;
    size_t cap;
    ssize_t head_node;
} ihash;

#define ihash_create(h, size_, cap_) \
    (typeof(h))ihash_create_fn( \
        size_, cap_, \
        offsetof(typeof(*h), key), \
        offsetof(typeof(*h), next), \
        sizeof(*h))

void ihash_free(void *hash);

#define ihash_get(h, key_) \
    (typeof(h))ihash_get_fn((ihash *)h, key_, offsetof(typeof(*h), key), offsetof(typeof(*h), next), sizeof(*h))

#define ihash_put(h, key_, value_) \
    (typeof(h))ihash_put_fn((ihash *)h, key_, offsetof(typeof(*h), key), offsetof(typeof(*h), next), \
            offsetof(typeof(*h), value), sizeof(value_), (void *)&value_, sizeof(*h)); \

/** Inner functions and macros */
ihash *ihash_create_fn(size_t size, size_t cap, size_t keyoffs,
        size_t nextoffs, size_t entrysz);
void *ihash_get_fn(ihash *hash, ssize_t key, size_t keyoffs, size_t nextoffs, size_t entrysz);
void *ihash_put_fn(ihash *hash, ssize_t key, size_t keyoffs, size_t nextoffs, size_t valueoffs, size_t valuesz, void *value, size_t entrysz);

#endif /* _IHASH_H_ */

