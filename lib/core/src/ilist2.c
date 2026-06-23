/**
 * @file ilist2.c
 * @brief Implementation of index-based doubly linked list
 *
 * This file implements a doubly linked list that uses indices instead of
 * pointers for node linking, making it suitable for shared memory and
 * persistent storage scenarios.
 *
 * Memory Layout:
 * ```
 * +-------------+
 * | ilist2      | Header (listsz, freelist_head, front_idx, back_idx, nodesz)
 * +-------------+
 * | User data   |
 * | prev idx    |
 * | next idx    |
 * +-------------+
 * | User data   |
 * | prev idx    |
 * | next idx    |
 * +-------------+
 * | ...         |
 * +-------------+
 * ```
 *
 * @section freelist Freelist Management
 * - freelist_head points to the first free node
 * - Each free node's next points to the next free node
 * - Free nodes form a singly linked list
 * - When a node is popped, it's added to the freelist
 * - When a node is pushed, it's taken from the freelist
 *
 * @see ilist2.h
 */


#include "ilist2.h"
#include <malloc.h>
#include <assert.h>

ilist2 *ilist2_create_fn(size_t listsz, size_t usersz);
ilist2 *ilist2_init_fn(void *p, size_t listsz, size_t usersz);
void ilist2_clear(void *p);
void ilist2_free(void *list);
void *ilist2_get_back_fn(ilist2 *list);
void *ilist2_get_front_fn(ilist2 *list);
void *ilist2_pop_back_fn(ilist2 *list);
void *ilist2_pop_front_fn(ilist2 *list);
void *ilist2_touch_back_fn(ilist2 *list, ilist2_idx_t *idx);
void *ilist2_touch_front_fn(ilist2 *list, ilist2_idx_t *idx);
void ilist2_move_back_by_idx_fn(ilist2 *list, ilist2_idx_t idx);
void ilist2_move_front_by_idx_fn(ilist2 *list, ilist2_idx_t idx);
static void *ilist2_touch_new_fn(ilist2 *list);

#ifndef NDEBUG
void ilist2_dump_list(void *p);
void ilist2_dump_idx(void *p);
void ilist2_dump_freelist(void *p);
#endif /* NDEBUG */

/**
 * @brief Internal function for list creation
 *
 * Allocates contiguous memory for list header and all nodes.
 *
 * @param listsz  Number of slots in the list
 * @param usersz  Size of each user's entry in bytes
 * @return        Pointer to initialized list, or NULL on allocation failure
 *
 * @see ilist2_create macro
 */
ilist2 *
ilist2_create_fn(size_t listsz, size_t usersz) {
    ilist2 *list;

    /* Allocate contiguous memory */
    list = malloc(ilist2_get_required_memory_size(listsz, usersz));

    return list ? ilist2_init_fn(list, listsz, usersz) : NULL;
}

/**
 * @brief Internal function for list initialization in pre-allocated memory
 *
 * @param p       Previously allocated pointer where list is going to be initialized
 * @param listsz  Number of slots in the list
 * @param usersz  Size of each user's entry in bytes
 * @return        Pointer to initialized list
 *
 * @see ilist2_init macro
 * @see ilist2_get_required_memory_size
 */
ilist2 *
ilist2_init_fn(void *p, size_t listsz, size_t usersz) {
    const size_t nodesz = usersz + sizeof(ilist2_idx_t) + sizeof(ilist2_idx_t);
    ilist2 *list = p;

    list->listsz = listsz;
    list->nodesz = nodesz;

    ilist2_clear(p);

    return list;
}

/**
 * @brief Clears all entries from the list
 *
 * Resets the list to empty state. All nodes are moved to the freelist.
 *
 * @param p Pointer to list to clear
 *
 * @note Time complexity: O(listsz)
 */
void
ilist2_clear(void *p) {
    ilist2 *list = p;
    const size_t nodesz = list->nodesz;
    const size_t listsz = list->listsz;
    const size_t nextoffs = nodesz - sizeof(ilist2_idx_t);
    const size_t prevoffs = nextoffs - sizeof(ilist2_idx_t);
    void *nodes = list->nodes;

    list->back_idx =    ILIST2_UNDEF;
    list->front_idx =   ILIST2_UNDEF;

    /* Initialize primary hash slots to empty state */
    for (size_t i = 0; i != listsz; ++i, nodes += nodesz) {
        *(ilist2_idx_t *)(nodes + prevoffs) = ILIST2_UNDEF;
        *(ilist2_idx_t *)(nodes + nextoffs) = i + 1;
    }

    /* Mark the end of the freelist */
    if (listsz > 0) {
        list->freelist_head = 0;
        *(ilist2_idx_t *)(nodes - nodesz + nextoffs) = ILIST2_UNDEF;
    } else
        list->freelist_head = ILIST2_UNDEF;
}

/**
 * @brief Frees a list created with ilist2_create()
 *
 * @param list Pointer to the list to free
 */
void
ilist2_free(void *list) {
    free((ilist2 *)list);
}

/**
 * @brief Internal function to get pointer to back node
 *
 * @param list Pointer to list
 * @return     Pointer to back node (user data), or NULL if list is empty
 *
 * @warning Does NOT check if list is empty
 */
void *
ilist2_get_back_fn(ilist2 *list) {
    return list->nodes + list->back_idx * list->nodesz;
}

/**
 * @brief Internal function to get pointer to front node
 *
 * @param list Pointer to list
 * @return     Pointer to front node (user data), or NULL if list is empty
 *
 * @warning Does NOT check if list is empty
 */
void *
ilist2_get_front_fn(ilist2 *list) {
    return list->nodes + list->front_idx * list->nodesz;
}

/**
 * @brief Internal function to pop back node
 *
 * @param list Pointer to list
 * @return     Pointer to popped node (user data), or NULL if list is empty
 *
 * @note The popped node is added to the freelist
 * @warning Returns NULL if list is empty
 */
void *
ilist2_pop_back_fn(ilist2 *list) {
    const size_t nodesz = list->nodesz;
    void *nodes = list->nodes;
    const size_t nextoffs = nodesz - sizeof(ilist2_idx_t);
    const size_t prevoffs = nextoffs - sizeof(ilist2_idx_t);
    void *old_node = nodes + list->back_idx * nodesz;
    const ilist2_idx_t prev_idx = *(ilist2_idx_t *)(old_node + prevoffs);

    *(ilist2_idx_t *)(old_node + nextoffs) = list->freelist_head; /* Mark old node's next node
                                                                     as freelist_head */

    if (prev_idx != ILIST2_UNDEF) {
        *(ilist2_idx_t *)(nodes + prev_idx * nodesz + nextoffs) =
            ILIST2_UNDEF;                                   /* update previous node's nextoffs */

        list->freelist_head = list->back_idx;           /* freelist_head = old node's index */
        list->back_idx = prev_idx;                                      /* update back_idx */
    } else {
        list->freelist_head = list->back_idx;
        list->front_idx = ILIST2_UNDEF;
        list->back_idx = ILIST2_UNDEF;
    }

    return old_node;
}

/**
 * @brief Internal function to pop front node
 *
 * @param list Pointer to list
 * @return     Pointer to popped node (user data), or NULL if list is empty
 *
 * @note The popped node is added to the freelist
 * @warning Returns NULL if list is empty
 */
void *
ilist2_pop_front_fn(ilist2 *list) {
    const size_t nodesz = list->nodesz;
    void *nodes = list->nodes;
    const size_t nextoffs = nodesz - sizeof(ilist2_idx_t);
    const size_t prevoffs = nextoffs - sizeof(ilist2_idx_t);
    void *old_node = nodes + list->front_idx * nodesz;
    const ilist2_idx_t next_idx = *(ilist2_idx_t *)(old_node + nextoffs);

    *(ilist2_idx_t *)(old_node + nextoffs) = list->freelist_head; /* Mark old node's next node
                                                                     as freelist_head */

    if (next_idx != ILIST2_UNDEF) {
        *(ilist2_idx_t *)(nodes + next_idx * nodesz + prevoffs) =
            ILIST2_UNDEF;                                   /* update next node's prevoffs */

        list->freelist_head = list->front_idx;          /* freelist_head = old node's index */
        list->front_idx = next_idx;                                     /* update front_idx */
    } else {
        list->freelist_head = list->front_idx;
        list->back_idx = ILIST2_UNDEF;
        list->front_idx = ILIST2_UNDEF;
    }

    return old_node;
}

/**
 * @brief Internal function to create a new node at the back
 *
 * @param list Pointer to list
 * @param idx  Pointer to store the index of the new node
 * @return     Pointer to the new node, or NULL if list is full
 *
 * @note If the list is full, returns NULL and idx is set to ILIST2_UNDEF
 */
void *
ilist2_touch_back_fn(ilist2 *list, ilist2_idx_t *idx) {
    *idx = list->freelist_head;

    if (*idx == ILIST2_UNDEF)
        return NULL;                                                /* No free nodes available */

    if (ilist2_empty(list))
        return ilist2_touch_new_fn(list);
    else {
        /* List is not empty */
        const size_t nodesz = list->nodesz;
        void *nodes = list->nodes;
        const size_t nextoffs = nodesz - sizeof(ilist2_idx_t);
        const size_t prevoffs = nextoffs - sizeof(ilist2_idx_t);

        ilist2_idx_t new_idx = *idx;

        void *back_node = nodes + list->back_idx * nodesz;          /* get back node */
        void *new_node = nodes + new_idx * nodesz;                  /* get new node */

        *(ilist2_idx_t *)(back_node + nextoffs) = new_idx; /* update previous node's nextoffs */

        list->freelist_head = *(ilist2_idx_t *)(new_node + nextoffs);  /* update freelist_head */

        *(ilist2_idx_t *)(new_node + prevoffs) = list->back_idx;    /* new node's prevoffs */
        *(ilist2_idx_t *)(new_node + nextoffs) = ILIST2_UNDEF;      /* new node's nextoffs */

        list->back_idx = new_idx;                                   /* update back_idx */

        return new_node;
    }
}

/**
 * @brief Internal function to create a new node at the front
 *
 * @param list Pointer to list
 * @param idx  Pointer to store the index of the new node
 * @return     Pointer to the new node, or NULL if list is full
 *
 * @note If the list is full, returns NULL and idx is set to ILIST2_UNDEF
 */
void *
ilist2_touch_front_fn(ilist2 *list, ilist2_idx_t *idx) {
    *idx = list->freelist_head;

    if (*idx == ILIST2_UNDEF)
        return NULL;                                                /* No free nodes available */

    if (ilist2_empty(list))
        return ilist2_touch_new_fn(list);
    else {
        /* List is not empty */
        const size_t nodesz = list->nodesz;
        void *nodes = list->nodes;
        const size_t nextoffs = nodesz - sizeof(ilist2_idx_t);
        const size_t prevoffs = nextoffs - sizeof(ilist2_idx_t);

        ilist2_idx_t new_idx = *idx;

        void *front_node = nodes + list->front_idx * nodesz;        /* get front node */
        void *new_node = nodes + new_idx * nodesz;                  /* get new node */

        *(ilist2_idx_t *)(front_node + prevoffs) = new_idx; /* update next node's prevoffs */

        list->freelist_head = *(ilist2_idx_t *)(new_node + nextoffs);  /* update freelist_head */

        *(ilist2_idx_t *)(new_node + prevoffs) = ILIST2_UNDEF;      /* new node's prevoffs */
        *(ilist2_idx_t *)(new_node + nextoffs) = list->front_idx;   /* new node's nextoffs */

        list->front_idx = new_idx;                                  /* update front_idx */

        return new_node;
    }
}

/**
 * @brief Internal function to move a node to the back
 *
 * @param list Pointer to list
 * @param idx  Index of the node to move
 *
 * @pre idx must be a valid index
 * @pre The node must be in the list
 *
 * @note If idx is already at the back, does nothing
 */
void
ilist2_move_back_by_idx_fn(ilist2 *list, ilist2_idx_t idx) {
    const size_t nodesz = list->nodesz;
    void *nodes = list->nodes;
    void *node = nodes + idx * nodesz;
    const size_t nextoffs = nodesz - sizeof(ilist2_idx_t);
    const size_t prevoffs = nextoffs - sizeof(ilist2_idx_t);

    const ilist2_idx_t prev_idx = *((ilist2_idx_t *)(node + prevoffs));
    const ilist2_idx_t next_idx = *((ilist2_idx_t *)(node + nextoffs));

    assert(idx >= 0 && (size_t)idx < list->listsz);

    /* Link together previous and next nodes */
    if (next_idx != ILIST2_UNDEF)
        *((ilist2_idx_t *)(nodes + next_idx * nodesz + prevoffs)) = prev_idx;
    else
        return; /* Node is already in back of the list */

    if (prev_idx != ILIST2_UNDEF)
        *((ilist2_idx_t *)(nodes + prev_idx * nodesz + nextoffs)) = next_idx;
    else
        /* next_idx is now front_idx */
        list->front_idx = next_idx;

    /* Update back node's nextoffs to point to idx */
    *((ilist2_idx_t *)(nodes + list->back_idx * nodesz + nextoffs)) = idx;

    /* Update node's prevoffs and nextoffs to point to new neighbours */
    *((ilist2_idx_t *)(node + prevoffs)) = list->back_idx;
    *((ilist2_idx_t *)(node + nextoffs)) = ILIST2_UNDEF;

    /* Update back_idx */
    list->back_idx = idx;
}

/**
 * @brief Internal function to move a node to the front
 *
 * @param list Pointer to list
 * @param idx  Index of the node to move
 *
 * @pre idx must be a valid index
 * @pre The node must be in the list
 *
 * @note If idx is already at the front, does nothing
 */
void
ilist2_move_front_by_idx_fn(ilist2 *list, ilist2_idx_t idx) {
    const size_t nodesz = list->nodesz;
    void *nodes = list->nodes;
    void *node = nodes + idx * nodesz;
    const size_t nextoffs = nodesz - sizeof(ilist2_idx_t);
    const size_t prevoffs = nextoffs - sizeof(ilist2_idx_t);

    const ilist2_idx_t prev_idx = *((ilist2_idx_t *)(node + prevoffs));
    const ilist2_idx_t next_idx = *((ilist2_idx_t *)(node + nextoffs));

    assert(idx >= 0 && (size_t)idx < list->listsz);

    /* Link together previous and next nodes */
    if (prev_idx != ILIST2_UNDEF)
        *((ilist2_idx_t *)(nodes + prev_idx * nodesz + nextoffs)) = next_idx;
    else
        return; /* Node is already in front of the list */

    if (next_idx != ILIST2_UNDEF)
        *((ilist2_idx_t *)(nodes + next_idx * nodesz + prevoffs)) = prev_idx;
    else
        /* prev_idx is now back_idx */
        list->back_idx = prev_idx;

    /* Update front node's prevoffs to point to idx */
    *((ilist2_idx_t *)(nodes + list->front_idx * nodesz + prevoffs)) = idx;

    /* Update node's prevoffs and nextoffs to point to new neighbours */
    *((ilist2_idx_t *)(node + prevoffs)) = ILIST2_UNDEF;
    *((ilist2_idx_t *)(node + nextoffs)) = list->front_idx;

    /* Update front_idx */
    list->front_idx = idx;
}

/**
 * @brief Internal function to create the first node in an empty list
 *
 * @param list Pointer to list
 * @return     Pointer to the new node
 *
 * @pre list must be empty
 * @pre freelist_head must be valid
 */
void *
ilist2_touch_new_fn(ilist2 *list) {
    /* Create the first node */
    const size_t nodesz = list->nodesz;
    void *nodes = list->nodes;
    const size_t nextoffs = nodesz - sizeof(ilist2_idx_t);
    const size_t prevoffs = nextoffs - sizeof(ilist2_idx_t);
    ilist2_idx_t new_idx = list->freelist_head;
    void *new_node = nodes + new_idx * nodesz;                      /* get new node */

    assert(new_idx != ILIST2_UNDEF);

    list->freelist_head = *(ilist2_idx_t *)(new_node + nextoffs);   /* update freelist_head */

    *(ilist2_idx_t *)(new_node + prevoffs) = ILIST2_UNDEF;          /* new node's prevoffs */
    *(ilist2_idx_t *)(new_node + nextoffs) = ILIST2_UNDEF;          /* new node's nextoffs */

    list->back_idx = new_idx;                                       /* update back_idx */
    list->front_idx = new_idx;                                      /* update front_idx */

    return new_node;
}

#ifndef NDEBUG

/**
 * @brief Prints the list structure (simple version)
 *
 * @param h Pointer to list
 *
 * @note Shows the actual list order with prev/next indices
 * @see ilist2_dump_idx()
 * @see ilist2_dump_freelist()
 */
void
ilist2_dump_list(void *p) {
    ilist2 *list = p;
    const size_t nodesz = list->nodesz;
    const void *nodes = list->nodes;
    const size_t nextoffs = nodesz - sizeof(ilist2_idx_t);
    const size_t prevoffs = nextoffs - sizeof(ilist2_idx_t);

    assert(nodesz != 0);

    if (list->front_idx == ILIST2_UNDEF) {
        printf("\n=== LIST ===\n");
        printf("-- List is empty --\n");
    } else {
        const void *node = nodes + nodesz * list->front_idx;

        printf("\n=== LIST (front_idx=%zu, back_idx=%zu, freelist=%zd) ===\n",
            list->front_idx, list->back_idx, list->freelist_head);

        for (;;) {
                ilist2_idx_t prev_idx = *((ilist2_idx_t *)(node + prevoffs));
                ilist2_idx_t next_idx = *((ilist2_idx_t *)(node + nextoffs));
                size_t idx = (node - nodes) / nodesz;

                prev_idx != ILIST2_UNDEF ? printf("[%3zu]  [%5zu]", idx, prev_idx) : printf("[%3zu]  [UNDEF]", idx);
                printf(" %i ", *(int *)node);
                next_idx != ILIST2_UNDEF ? printf("[%5zu]\n", next_idx) : printf("[UNDEF]\n");

                if (*((ilist2_idx_t *)(node + nextoffs)) == ILIST2_UNDEF)
                    break;

                node = nodes + nodesz * (*((ilist2_idx_t *)(node + nextoffs)));
        }
    }

    printf("=================================\n");
}

/**
 * @brief Prints all nodes with their prev/next indices
 *
 * @param h Pointer to list
 *
 * @note Shows every slot in the list, including free nodes
 * @see ilist2_dump_list()
 * @see ilist2_dump_freelist()
 */
void
ilist2_dump_idx(void *p) {
    ilist2 *list = p;
    const size_t nodesz = list->nodesz;
    const void *nodes = list->nodes;
    const size_t nextoffs = nodesz - sizeof(ilist2_idx_t);
    const size_t prevoffs = nextoffs - sizeof(ilist2_idx_t);

    if (list->front_idx == ILIST2_UNDEF) {
        printf("\n=== LIST IDX ===\n");
        printf("-- List is empty --\n");
    } else {
        const void *node = nodes;

        printf("\n=== LIST IDX (front_idx=%zu, back_idx=%zu, freelist=%zd) ===\n",
            list->front_idx, list->back_idx, list->freelist_head);

        for (size_t i = 0, ie = list->listsz; i != ie; ++i) {
                ilist2_idx_t prev_idx = *((ilist2_idx_t *)(node + prevoffs));
                ilist2_idx_t next_idx = *((ilist2_idx_t *)(node + nextoffs));

                prev_idx != ILIST2_UNDEF ? printf("[%3zu]  [%5zu]", i, prev_idx) : printf("[%3zu]  [UNDEF]", i);
                printf(" %i ", *(int *)node);
                next_idx != ILIST2_UNDEF ? printf("[%5zu]\n", next_idx) : printf("[UNDEF]\n");

                node += nodesz;
        }
    }

    printf("=================================\n");
}

/**
 * @brief Prints the freelist chain
 *
 * @param h Pointer to list
 *
 * @note Shows the linked list of free nodes available for reuse
 * @see ilist2_dump_list()
 * @see ilist2_dump_idx()
 */
void ilist2_dump_freelist(void *p) {
    ilist2 *list = p;
    ssize_t count = 0;
    const size_t listsz = list->listsz;
    ilist2_idx_t idx = list->freelist_head;
    const size_t nodesz = list->nodesz;
    const void *nodes = list->nodes;
    const size_t nextoffs = nodesz - sizeof(ilist2_idx_t);

    printf("Freelist:(front_idx=%zd, back_idx=%zd, freelist=%zd) ===\n",
        list->front_idx, list->back_idx, list->freelist_head);

    while (idx != ILIST2_UNDEF) {
        if (idx < 0 || idx >= (ilist2_idx_t)listsz) {
            printf("ERROR: freelist index %zd out of range\n", idx);
            break;
        }
        printf("%zd -> ", idx);
        count++;
        if (count > 1000) break;
        const char *node = nodes + idx * nodesz;
        idx = *(ilist2_idx_t *)(node + nextoffs);
    }
    printf("ILIST2_UNDEF, ");
    printf("Freelist contains %zd nodes\n", count);
}

#endif /* NDEBUG */

