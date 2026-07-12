/**
 * @file test_memory.c
 * @brief Unit tests for memory context manager
 *
 * Tests bump and linear allocators, context hierarchy,
 * and memory management operations.
 */

#include "quin.h"
#include "memory.h"
#include <unistd.h>
#include <string.h>

/* Helper: fill memory with pattern */
static void fill_pattern(void *ptr, size_t size, unsigned char pattern) {
    memset(ptr, pattern, size);
}

/* Helper: check pattern */
static int check_pattern(void *ptr, size_t size, unsigned char pattern) {
    unsigned char *p = (unsigned char *)ptr;
    for (size_t i = 0; i < size; i++) {
        if (p[i] != pattern) return 0;
    }
    return 1;
}

/* Helper: returns number of children */
static int child_num(BumpContext *context) {
    int cnt = 0;
    for (int i = 0; i != CONTEXT_MAX_CHILDREN; ++i){
        if (context->children[i] != NULL)
            ++cnt;
    }

    return cnt;
}

TEST(memory)

    TEST_SUITE(memory_init)

        TEST_CASE(init) {
            memory_init_default();
            TEST_CHECK(MEMORY_PAGESZ > 0);
            TEST_CHECK(MEMORY_PAGESZ == (size_t)sysconf(_SC_PAGESIZE));
            memory_destroy();
        }

    TEST_SUITE_END()

    TEST_SUITE(bump_context)

        TEST_CASE(create) {
            memory_init(bump_context_create(MEMORY_PAGESZ));

            TEST_CHECK(g_context != NULL);

            memory_destroy();
        }

        TEST_CASE(create_zero_size) {
            memory_init(bump_context_create(0));
            /* Should handle zero size gracefully */
            TEST_CHECK(g_context == NULL);

            memory_destroy();
        }

        TEST_CASE(alloc_basic) {
            memory_init_default();
            TEST_CHECK(g_context != NULL);

            int *p = context_alloc(g_context, sizeof(int));
            TEST_CHECK(p != NULL);
            *p = 42;
            TEST_CHECK(*p == 42);

            memory_destroy();
        }

        TEST_CASE(alloc_multiple) {
            memory_init_default();
            Context *ctx = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(ctx != NULL);

            int *p1 = context_alloc(ctx, sizeof(int));
            int *p2 = context_alloc(ctx, sizeof(int));
            int *p3 = context_alloc(ctx, sizeof(int));

            TEST_CHECK(p1 != NULL);
            TEST_CHECK(p2 != NULL);
            TEST_CHECK(p3 != NULL);

            *p1 = 1;
            *p2 = 2;
            *p3 = 3;

            TEST_CHECK(*p1 == 1);
            TEST_CHECK(*p2 == 2);
            TEST_CHECK(*p3 == 3);

            /* Check that pointers are different and sequential */
            TEST_CHECK((char *)p2 - (char *)p1 > 0);
            TEST_CHECK((char *)p3 - (char *)p2 > 0);

            context_drop(ctx);
            memory_destroy();
        }

        TEST_CASE(alloc_large) {
            memory_init_default();
            Context *ctx = bump_context_create(MEMORY_PAGESZ * 4);
            TEST_CHECK(ctx != NULL);

            size_t large_size = MEMORY_PAGESZ * 2;
            char *p = context_alloc(ctx, large_size);
            TEST_CHECK(p != NULL);

            fill_pattern(p, large_size, 0xAA);
            TEST_CHECK(check_pattern(p, large_size, 0xAA));

            context_drop(ctx);
            memory_destroy();
        }

        TEST_CASE(alloc_exceed) {
            memory_init_default();
            Context *ctx = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(ctx != NULL);

            /* Allocate almost all memory */
                void *p1 = context_alloc(ctx, MEMORY_PAGESZ - sizeof(BumpContext) - 24);
            TEST_CHECK(p1 != NULL);

            /* Next allocation should fail */
            void *p2 = context_alloc(ctx, 128);
            TEST_CHECK(p2 == NULL);

            context_drop(ctx);
            memory_destroy();
        }

        TEST_CASE(alloc_zero) {
            memory_init_default();
            Context *ctx = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(ctx != NULL);

            void *p = context_alloc(ctx, 0);
            TEST_CHECK(p == NULL);

            context_drop(ctx);
            memory_destroy();
        }

    TEST_SUITE_END()

    TEST_SUITE(bump_realloc)

        TEST_CASE(realloc_grow) {
            memory_init_default();
            Context *ctx = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(ctx != NULL);

            int *p = context_alloc(ctx, sizeof(int));
            *p = 42;

            int *np = context_realloc(ctx, p, sizeof(int) * 3);
            TEST_CHECK(np != NULL);
            TEST_CHECK(*np == 42);

            np[1] = 7;
            np[2] = 9;
            TEST_CHECK(np[0] == 42);
            TEST_CHECK(np[1] == 7);
            TEST_CHECK(np[2] == 9);

            context_drop(ctx);
            memory_destroy();
        }

        TEST_CASE(realloc_shrink) {
            memory_init_default();
            Context *ctx = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(ctx != NULL);

            int *p = context_alloc(ctx, sizeof(int) * 10);
            for (int i = 0; i < 10; i++) p[i] = i;

            int *np = context_realloc(ctx, p, sizeof(int) * 3);
            TEST_CHECK(np != NULL);
            TEST_CHECK(np[0] == 0);
            TEST_CHECK(np[1] == 1);
            TEST_CHECK(np[2] == 2);

            context_drop(ctx);
            memory_destroy();
        }

        TEST_CASE(realloc_null) {
            memory_init_default();
            Context *ctx = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(ctx != NULL);

            /* realloc(NULL, size) should behave like alloc */
            int *p = context_realloc(ctx, NULL, sizeof(int));
            TEST_CHECK(p != NULL);
            *p = 42;
            TEST_CHECK(*p == 42);

            context_drop(ctx);
            memory_destroy();
        }

        TEST_CASE(realloc_zero) {
            memory_init_default();
            Context *ctx = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(ctx != NULL);

            int *p = context_alloc(ctx, sizeof(int));
            *p = 42;

            int *np = context_realloc(ctx, p, 0);
            TEST_CHECK(np == NULL);

            context_drop(ctx);
            memory_destroy();
        }

    TEST_SUITE_END()

    TEST_SUITE(bump_free)

        TEST_CASE(free_tail) {
            memory_init_default();
            Context *ctx = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(ctx != NULL);

            int *p1 = context_alloc(ctx, sizeof(int));
            int *p2 = context_alloc(ctx, sizeof(int));
            int *p3 = context_alloc(ctx, sizeof(int));

            *p1 = 1;
            *p2 = 2;
            *p3 = 3;

            /* Free tail (p3) should work */
            context_free(ctx, p3);

            /* Should be able to allocate again */
            int *p4 = context_alloc(ctx, sizeof(int));
            TEST_CHECK(p4 != NULL);
            *p4 = 4;
            TEST_CHECK(*p4 == 4);

            context_drop(ctx);
            memory_destroy();
        }

        TEST_CASE(free_null) {
            memory_init_default();
            Context *ctx = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(ctx != NULL);

            /* Freeing NULL should be no-op */
            context_free(ctx, NULL);

            context_drop(ctx);
            memory_destroy();
        }

        TEST_CASE(free_non_tail) {
            memory_init_default();
            Context *ctx = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(ctx != NULL);

            int *p1 = context_alloc(ctx, sizeof(int));
            int *p2 = context_alloc(ctx, sizeof(int));
            int *p3 = context_alloc(ctx, sizeof(int));

            *p1 = 1;
            *p2 = 2;
            *p3 = 3;

            /* Free non-tail (p2) should not reduce current */
            context_free(ctx, p2);

            /* p1 and p3 should still be valid */
            TEST_CHECK(*p1 == 1);
            TEST_CHECK(*p3 == 3);

            context_drop(ctx);
            memory_destroy();
        }

    TEST_SUITE_END()

    TEST_SUITE(bump_reset)

        TEST_CASE(reset) {
            memory_init_default();
            Context *ctx = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(ctx != NULL);

            int *p = context_alloc(ctx, sizeof(int) * 100);
            fill_pattern(p, sizeof(int) * 100, 0xBB);

            /* Reset should clear all allocations */
            context_reset(ctx);

            /* Should be able to allocate again */
            int *np = context_alloc(ctx, sizeof(int));
            TEST_CHECK(np != NULL);
            *np = 42;
            TEST_CHECK(*np == 42);

            context_drop(ctx);
            memory_destroy();
        }

        TEST_CASE(reset_with_children) {
            memory_init_default();
            Context *parent = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(parent != NULL);

            Context *child = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(child != NULL);

            TEST_CHECK(child_num((BumpContext *)parent) == 0);

            context_add_child(parent, child);

            TEST_CHECK(child_num((BumpContext *)parent) == 1);

            int *p = context_alloc(child, sizeof(int));
            *p = 42;

            /* Reset parent should also drop children */
            context_reset(parent);

            TEST_CHECK(child_num((BumpContext *)parent) == 0);

            context_drop(parent);
            memory_destroy();
        }

    TEST_SUITE_END()

    TEST_SUITE(bump_drop)

        TEST_CASE(drop) {
            memory_init_default();
            Context *ctx = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(ctx != NULL);

            int *p = context_alloc(ctx, sizeof(int));
            *p = 42;

            int ret = context_drop(ctx);
            TEST_CHECK(ret == 0);

            /* Context should be freed */
            memory_destroy();
        }

        TEST_CASE(drop_with_children) {
            memory_init_default();
            Context *parent = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(parent != NULL);

            Context *child = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(child != NULL);

            context_add_child(parent, child);

            int *p = context_alloc(child, sizeof(int));
            *p = 42;

            /* Drop parent should drop all children */
            int ret = context_drop(parent);
            TEST_CHECK(ret == 0);

            /* Children should be automatically dropped */
            memory_destroy();
        }

    TEST_SUITE_END()

    TEST_SUITE(bump_children)

        TEST_CASE(add_child) {
            memory_init_default();
            Context *parent = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(parent != NULL);

            Context *child = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(child != NULL);

            TEST_CHECK(child_num((BumpContext *)parent) == 0);

            Context *result = context_add_child(parent, child);
            TEST_CHECK(result == child);

            TEST_CHECK(child_num((BumpContext *)parent) == 1);

            context_drop(parent);
            memory_destroy();
        }

        TEST_CASE(add_child_multiple) {
            memory_init_default();
            Context *parent = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(parent != NULL);

            Context *children[CONTEXT_MAX_CHILDREN];
            for (int i = 0; i < CONTEXT_MAX_CHILDREN; i++) {
                children[i] = bump_context_create(MEMORY_PAGESZ);
                TEST_CHECK(children[i] != NULL);
                Context *result = context_add_child(parent, children[i]);
                TEST_CHECK(result == children[i]);
                TEST_CHECK(child_num((BumpContext *)parent) == i + 1);
            }

            /* Next child should fail (max children reached) */
            Context *extra = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(extra != NULL);
            Context *result = context_add_child(parent, extra);
            TEST_CHECK(result == NULL);

            context_drop(extra);
            context_drop(parent);
            memory_destroy();
        }

        TEST_CASE(parent_child_allocation) {
            memory_init_default();
            Context *parent = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(parent != NULL);

            Context *child = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(child != NULL);

            context_add_child(parent, child);

            /* Allocate in child context */
            int *p = context_alloc(child, sizeof(int));
            TEST_CHECK(p != NULL);
            *p = 42;
            TEST_CHECK(*p == 42);

            context_drop(parent);
            memory_destroy();
        }

    TEST_SUITE_END()

    TEST_SUITE(linear_context)

        TEST_CASE(create) {
            memory_init_default();
            Context *ctx = linear_context_create(MEMORY_PAGESZ);
            TEST_CHECK(ctx != NULL);

            context_drop(ctx);
            memory_destroy();
        }

        TEST_CASE(alloc_basic) {
            memory_init_default();
            Context *ctx = linear_context_create(MEMORY_PAGESZ);
            TEST_CHECK(ctx != NULL);

            int *p = context_alloc(ctx, sizeof(int));
            TEST_CHECK(p != NULL);
            *p = 42;
            TEST_CHECK(*p == 42);

            context_drop(ctx);
            memory_destroy();
        }

        TEST_CASE(alloc_multiple) {
            memory_init_default();
            Context *ctx = linear_context_create(MEMORY_PAGESZ);
            TEST_CHECK(ctx != NULL);

            int *p1 = context_alloc(ctx, sizeof(int));
            int *p2 = context_alloc(ctx, sizeof(int));
            int *p3 = context_alloc(ctx, sizeof(int));

            TEST_CHECK(p1 != NULL);
            TEST_CHECK(p2 != NULL);
            TEST_CHECK(p3 != NULL);

            *p1 = 1;
            *p2 = 2;
            *p3 = 3;

            TEST_CHECK(*p1 == 1);
            TEST_CHECK(*p2 == 2);
            TEST_CHECK(*p3 == 3);

            context_drop(ctx);
            memory_destroy();
        }

        TEST_CASE(alloc_exceed) {
            memory_init_default();
            Context *ctx = linear_context_create(MEMORY_PAGESZ);
            TEST_CHECK(ctx != NULL);

            /* Allocate almost all memory */
            void *p1 = context_alloc(ctx, MEMORY_PAGESZ - sizeof(BumpContext) - 16);
            TEST_CHECK(p1 != NULL);

            /* Next allocation should fail */
            void *p2 = context_alloc(ctx, 128);
            TEST_CHECK(p2 == NULL);

            context_drop(ctx);
            memory_destroy();
        }

        TEST_CASE(linear_no_realloc) {
            memory_init_default();
            Context *ctx = linear_context_create(MEMORY_PAGESZ);
            TEST_CHECK(ctx != NULL);

            int *p = context_alloc(ctx, sizeof(int));
            *p = 42;

            /* realloc should fail (assert in debug, return NULL in release) */

            /* @todo: I should uncomment the following test */
            /*
            int *np = context_realloc(ctx, p, sizeof(int) * 2);
            TEST_CHECK(np == NULL);
            */

            context_drop(ctx);
            memory_destroy();
        }

        TEST_CASE(linear_no_free) {
            memory_init_default();
            Context *ctx = linear_context_create(MEMORY_PAGESZ);
            TEST_CHECK(ctx != NULL);

            int *p = context_alloc(ctx, sizeof(int));
            *p = 42;

            /* free should be no-op (or assert) */
            /* @todo: I should uncomment the following test */
            /*
            context_free(ctx, p);
            */

            context_drop(ctx);
            memory_destroy();
        }

        TEST_CASE(linear_reset) {
            memory_init_default();
            Context *ctx = linear_context_create(MEMORY_PAGESZ);
            TEST_CHECK(ctx != NULL);

            int *p = context_alloc(ctx, sizeof(int) * 100);
            fill_pattern(p, sizeof(int) * 100, 0xCC);

            /* Reset should clear all allocations */
            context_reset(ctx);

            /* Should be able to allocate again */
            int *np = context_alloc(ctx, sizeof(int));
            TEST_CHECK(np != NULL);
            *np = 42;
            TEST_CHECK(*np == 42);

            context_drop(ctx);
            memory_destroy();
        }

    TEST_SUITE_END()

    TEST_SUITE(global_context)

        TEST_CASE(set_get) {
            memory_init_default();
            Context *ctx = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(ctx != NULL);

            Context *old = context_switch(ctx);
            TEST_CHECK(old == NULL || old != ctx);

            Context *current = context_get_current();
            TEST_CHECK(current == ctx);

            context_switch(old);
            context_drop(ctx);
            memory_destroy();
        }

        TEST_CASE(global_alloc) {
            memory_init_default();
            Context *ctx = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(ctx != NULL);

            context_switch(ctx);

            int *p = salloc(sizeof(int));
            TEST_CHECK(p != NULL);
            *p = 42;
            TEST_CHECK(*p == 42);

            context_drop(ctx);
            memory_destroy();
        }

        TEST_CASE(global_realloc) {
            memory_init_default();
            Context *ctx = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(ctx != NULL);

            context_switch(ctx);

            int *p = salloc(sizeof(int));
            *p = 42;

            int *np = srealloc(p, sizeof(int) * 3);
            TEST_CHECK(np != NULL);
            TEST_CHECK(np[0] == 42);

            context_drop(ctx);
            memory_destroy();
        }

        TEST_CASE(global_free) {
            memory_init_default();
            Context *ctx = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(ctx != NULL);

            context_switch(ctx);

            int *p = salloc(sizeof(int));
            *p = 42;

            sfree(p);

            /* Should be able to allocate again */
            int *np = salloc(sizeof(int));
            TEST_CHECK(np != NULL);

            context_drop(ctx);
            memory_destroy();
        }

        TEST_CASE(global_switch_restore) {
            memory_init(bump_context_create(MEMORY_PAGESZ));
            Context *ctx1 = g_context;
            Context *ctx2 = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(ctx1 != NULL);
            TEST_CHECK(ctx2 != NULL);

            context_switch(ctx1);
            int *p1 = salloc(sizeof(int));
            *p1 = 1;

            context_switch(ctx2);
            int *p2 = salloc(sizeof(int));
            *p2 = 2;

            context_switch(ctx1);
            TEST_CHECK(*p1 == 1);

            context_switch(ctx2);
            TEST_CHECK(*p2 == 2);

            context_drop(ctx2);
            memory_destroy();
        }

    TEST_SUITE_END()

    TEST_SUITE(memory_stress)

        TEST_CASE(many_allocations) {
            memory_init_default();
            Context *ctx = bump_context_create(MEMORY_PAGESZ * 4);
            TEST_CHECK(ctx != NULL);

            const int NUM_ALLOCS = 100;
            void *ptrs[NUM_ALLOCS];

            for (int i = 0; i < NUM_ALLOCS; i++) {
                size_t size = (i % 10 + 1) * 8;
                ptrs[i] = context_alloc(ctx, size);
                TEST_CHECK(ptrs[i] != NULL);
                fill_pattern(ptrs[i], size, (unsigned char)(i & 0xFF));
            }

            for (int i = 0; i < NUM_ALLOCS; i++) {
                size_t size = (i % 10 + 1) * 8;
                TEST_CHECK(check_pattern(ptrs[i], size, (unsigned char)(i & 0xFF)));
            }

            context_drop(ctx);
            memory_destroy();
        }

        TEST_CASE(nested_contexts) {
            memory_init_default();
            const int DEPTH = 5;
            Context *contexts[DEPTH];

            contexts[0] = bump_context_create(MEMORY_PAGESZ);
            TEST_CHECK(contexts[0] != NULL);

            for (int i = 1; i < DEPTH; i++) {
                contexts[i] = bump_context_create(MEMORY_PAGESZ);
                TEST_CHECK(contexts[i] != NULL);
                context_add_child(contexts[i-1], contexts[i]);
            }

            /* Allocate in each context */
            for (int i = 0; i < DEPTH; i++) {
                int *p = context_alloc(contexts[i], sizeof(int));
                TEST_CHECK(p != NULL);
                *p = i;
                TEST_CHECK(*p == i);
            }

            /* Drop all contexts (should cascade) */
            context_drop(contexts[0]);
            memory_destroy();
        }

    TEST_SUITE_END()

TEST_END()

