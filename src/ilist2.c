#include "ilist2.h"
#include <malloc.h>
#include <string.h>
#include <assert.h>

#define ilist2_get_nodes(h)   ((void *)((char *)h + sizeof(ilist2)))

void ilist2_free(void *list);
ilist2 *ilist2_create_fn(size_t listsz, size_t usersz);
ilist2 *ilist2_init_fn(void *p, size_t list, size_t usersz);
void ilist2_clear(void *p);
void *ilist2_get_back_fn(ilist2 *list);
void *ilist2_pop_back_fn(ilist2 *list);
void *ilist2_touch_back_fn(ilist2 *list);

#ifndef NDEBUG
void ilist2_dump_simple(void *h);
void ilist2_dump_debug(void *h);
void ilist2_dump_freelist(void *h);
#endif /* NDEBUG */

void
ilist2_free(void *list) {
    free((ilist2 *)list);
}

ilist2 *
ilist2_create_fn(size_t listsz, size_t usersz) {
    /* Allocate contiguous memory */
    ilist2 *list;

    /* Allocate contiguous memory */
    list = malloc(ilist2_get_required_memory_size(listsz, usersz));

    return list ? ilist2_init_fn(list, listsz, usersz) : NULL;
}

ilist2 *
ilist2_init_fn(void *p, size_t listsz, size_t usersz) {
    const size_t nodesz = usersz + sizeof(ilist2_idx_t) + sizeof(ilist2_idx_t);
    ilist2 *list = p;

    list->listsz = listsz;
    list->nodesz = nodesz;

    ilist2_clear(p);

    return list;
}

void
ilist2_clear(void *p) {
    ilist2 *list = p;
    const size_t nodesz = list->nodesz;
    const size_t listsz = list->listsz;
    const size_t nextoffs = nodesz - sizeof(ilist2_idx_t);
    const size_t prevoffs = nextoffs - 1;
    void *e = ilist2_get_nodes(list);

    list->back_idx =    ILIST2_UNDEF;
    list->front_idx =   ILIST2_UNDEF;

    /* Initialize primary hash slots to empty state */
    for (size_t i = 0; i != listsz; ++i, e += nodesz) {
        *(ssize_t *)(e + prevoffs) = ILIST2_UNDEF;
        *(ssize_t *)(e + nextoffs) = i + 1;
    }

    /* Mark the end of the freelist */
    if (listsz > 0) {
        list->freelist_head = 0;
        *(ssize_t *)(e - nodesz + nextoffs) = ILIST2_UNDEF;
    } else
        list->freelist_head = ILIST2_UNDEF;
}

void *
ilist2_get_back_fn(ilist2 *list) {
    return ilist2_get_nodes(list) + list->back_idx * list->nodesz;
}

void *
ilist2_pop_back_fn(ilist2 *list) {
    const size_t nodesz = list->nodesz;
    void *nodes = ilist2_get_nodes(list);
    const size_t nextoffs = nodesz - sizeof(ilist2_idx_t);
    const size_t prevoffs = nextoffs - 1;
    void *old_node = nodes + list->back_idx * nodesz;
    const ilist2_idx_t prev_idx = *(ilist2_idx_t *)(old_node + prevoffs);

    *(ilist2_idx_t *)(nodes + prev_idx * nodesz) = ILIST2_UNDEF;    /* update previous node */

    *(ilist2_idx_t *)(old_node + nextoffs) = ILIST2_UNDEF;
    list->freelist_head = (nodes - old_node) / nodesz;  /* freelist_head = old node's index */
    list->back_idx = prev_idx;                                      /* update back_idx */

    return old_node;
}

void *
ilist2_touch_back_fn(ilist2 *list) {
    const size_t nodesz = list->nodesz;
    void *nodes = ilist2_get_nodes(list);
    const size_t nextoffs = nodesz - sizeof(ilist2_idx_t);
    const size_t prevoffs = nextoffs - 1;

    if (list->freelist_head == ILIST2_UNDEF)
        return NULL;                                                /* No free nodes available */

    if (list->back_idx != ILIST2_UNDEF) {
        void *back_node;
        void *new_node;
        ilist2_idx_t new_idx = list->freelist_head;

        if (new_idx == ILIST2_UNDEF)
            return NULL;

        back_node = nodes + list->back_idx * nodesz;                /* get back node */
        new_node = nodes + new_idx * nodesz;                        /* get new node */

        *(size_t *)(back_node + nextoffs) = new_idx; /* update previous node's nextoffs */

        list->freelist_head = *(ilist2_idx_t *)(new_node + nextoffs);  /* update freelist_head */
        list->back_idx = new_idx;                                   /* update back_idx */

        *(ilist2_idx_t *)(new_node + prevoffs) = list->back_idx;    /* new node's prevoffs */
        *(ilist2_idx_t *)(new_node + nextoffs) = ILIST2_UNDEF;      /* new node's nextoffs */

        return new_node;
    } else {
        /* Create the first node */
        ilist2_idx_t new_idx = list->freelist_head;
        void *new_node;

        if (new_idx == ILIST2_UNDEF)
            return NULL;

        new_node = nodes + new_idx * nodesz;                        /* get new node */

        *(size_t *)(new_node + nextoffs) = new_idx;
        list->freelist_head = *(ilist2_idx_t *)(new_node + nextoffs);  /* update freelist_head */
        list->back_idx = new_idx;                                   /* update back_idx */
        list->front_idx = new_idx;                                  /* update front_idx */

        *(ilist2_idx_t *)(new_node + nextoffs) = ILIST2_UNDEF;      /* new node's prevoffs */
        *(ilist2_idx_t *)(new_node + prevoffs) = ILIST2_UNDEF;      /* new node's nextoffs */

        return new_node;
    }
}

#ifndef NDEBUG

#if 0

void
ilist2_dump_simple(void *h) {
    ilist2 *hash = h;
    const size_t nodesz = hash->nodesz;
    const size_t keyoffs = hash->keyoffs;
    const size_t nextoffs = nodesz - sizeof(ilist2_idx_t);
    const size_t total_buckets = hash->bucketsz;
    char *buckets = (char *)ilist2_get_buckets(hash);
    char *chains = (char *)ilist2_get_chains(hash);

    printf("\n=== HASH TABLE (buckets=%zu, chains=%zu, freelist=%zd) ===\n",
           hash->bucketsz, hash->chainsz, hash->freelist_head);

    for (size_t i = 0; i < total_buckets; ++i) {
        char *bucket = buckets + i * nodesz;
        ssize_t key = *(ssize_t *)(bucket + keyoffs);
        ssize_t next = *(ssize_t *)(bucket + nextoffs);

        if (key == ILIST2_UNDEF) {
            printf("  [%3zu] EMPTY\n", i);
        } else {
            printf("  [%3zu] key=%4ld", i, key);

            /* Traverse chain */
            ssize_t chain_idx = next;
            while (chain_idx != ILIST2_UNDEF) {
                char *chain_node = chains + chain_idx * nodesz;
                ssize_t chain_key = *(ssize_t *)(chain_node + keyoffs);
                printf(" -> %ld", chain_key);
                chain_idx = *(ssize_t *)(chain_node + nextoffs);
            }
            printf("\n");
        }
    }
    printf("=================================\n");
}


void
ilist2_dump_debug(void *h) {
    ilist2 *hash = h;
    const size_t nodesz = hash->nodesz;
    const size_t keyoffs = hash->keyoffs;
    const size_t nextoffs = nodesz - sizeof(ilist2_idx_t);
    const size_t total_buckets = hash->bucketsz;
    const size_t total_nodes = hash->bucketsz + hash->chainsz;
    char *buckets = (char *)ilist2_get_buckets(hash);
    char *chains = (char *)ilist2_get_chains(hash);

    printf("\n=== HASH TABLE DEBUG ===\n");
    printf("bucketsz=%zu, chainsz=%zu, total_slots=%zu\n",
           hash->bucketsz, hash->chainsz, total_nodes);
    printf("keyoffs=%zu, nextoffs=%zu, nodesz=%zu\n",
           keyoffs, nextoffs, nodesz);
    printf("freelist_head=%zd\n\n", hash->freelist_head);

    /* Dump all slots (buckets + chains) */
    for (size_t i = 0; i < total_nodes; ++i) {
        char *slot = buckets + i * nodesz;
        ssize_t key = *(ssize_t *)(slot + keyoffs);
        ssize_t next = *(ssize_t *)(slot + nextoffs);
        const char *type = (i < hash->bucketsz) ? "BUCKET" : "CHAIN";

        if (key == ILIST2_UNDEF) {
            printf("%s[%3zu]: EMPTY (next=%zd)\n", type, i, next);
        } else {
            printf("%s[%3zu]: key=%4ld (next=%zd)\n", type, i, key, next);
        }
    }

    /* Dump freelist */
    if (hash->freelist_head != ILIST2_UNDEF) {
        printf("\nFreelist: ");
        ssize_t idx = hash->freelist_head;
        while (idx != ILIST2_UNDEF && idx < (ssize_t)total_nodes) {
            printf("%zd", idx);
            char *node = chains + idx * nodesz;
            idx = *(ssize_t *)(node + nextoffs);
            printf(" -> ");
            if (idx == hash->freelist_head) break; /* loop detection */
        }
        printf("ILIST2_UNDEF\n");
    }

    printf("========================\n");
}


void ilist2_dump_freelist(void *h) {
    ilist2 *hash = h;
    ssize_t count = 0;
    ssize_t idx = hash->freelist_head;
    const size_t nodesz = hash->nodesz;
    const size_t nextoffs = nodesz - sizeof(ilist2_idx_t);
    void *chains = ilist2_get_chains(hash);

    printf("Freelist: ");
    while (idx != ILIST2_UNDEF) {
        if (idx < 0 || idx >= hash->chainsz) {
            printf("ERROR: freelist index %zd out of range\n", idx);
            break;
        }
        printf("%zd -> ", idx);
        count++;
        if (count > 1000) break;
        char *node = chains + idx * hash->nodesz;
        idx = *(ssize_t *)(node + nextoffs);
    }
    printf("ILIST2_UNDEF, ");
    printf("Freelist contains %zd nodes\n", count);
}

#endif

#endif /* NDEBUG */

