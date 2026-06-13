/**
 * @file basic.c
 * @brief Unit tests for the grid storage system
 *
 * This file contains comprehensive unit tests demonstrating and verifying
 * the functionality of the grid-based data storage system.
 *
 * @section test_framework Test Framework Overview
 *
 * The test framework uses the following macros:
 * - TEST(name) / TEST_END(): Defines the main test unit (one pair per file)
 * - TEST_SUITE_BEGIN(name) / TEST_SUITE_END(): Groups related test cases
 * - TEST_CASE(name) { ... }: Individual test case with a code block
 * - TEST_REQUIRE(condition): Asserts that condition must be true (fails test if false)
 * - TEST_CHECK(condition): Checks a condition and records pass/fail statistics
 * - TEST_FAIL(): Explicitly marks the current test case as failed
 *
 * Test Flow:
 * 1. TEST(name) - TEST_END() pair contains one or more test suites
 * 2. Each test suite contains zero or more test cases
 * 3. Each test case contains one or more TEST_REQUIRE(), TEST_CHECK(). TEST_OK or TEST_FAUL assertions
 *
 * @note A test case fails if any TEST_REQUIRE (or TEST_CHECK) condition evaluates to 0 (false) or
 *  TEST_FAIL is called
 * @note TEST_CHECK() records statistics but does not stop test execution
 * @note All non-zero values are considered successful (passed)
 *
 * @author Based on Quin testing framework
 */

#include "quin.h"
#include "grid.h"
#include "ilist2.h"
#include "ihash.h"
#include <malloc.h>
#include <string.h>

TEST(basic)

    /**
     * @brief Test suite for grid operations
     *
     * This suite focuses on testing the core grid functionality including
     * header grid management, column definitions, and data grid operations.
     */
    TEST_SUITE(grid)

        TEST_CASE(test_1) {
            /* Allocate memory pages for header grid and data grid */
            Page *hp = malloc(8096), *p = malloc(8096);
            Grid *hg = hgrid_init(hp, 8096, GT_FIXED), *g;
            HColumn *hc;
            Row row;

            /* ===== Schema Definition (DDL Operations) ===== */

            /* CREATE TABLE HEADER - Initialize empty schema */

            /* ALTER TABLE ADD COLUMN - Add first column to schema */
            if (!hgrid_add_column(hg, "id_user", 4)) {
                TEST_FAIL();  /* Column addition failed - critical error */
            }
            /* Verify that one column was successfully added */
            TEST_CHECK(hg->occupied == 1);

            /* ALTER TABLE ADD COLUMN - Add second column to schema */
            if (!hgrid_add_column(hg, "name", 12)) {
                TEST_FAIL();  /* Column addition failed - critical error */
            }
            /* Verify that both columns are now present */
            TEST_CHECK(hg->occupied == 2);

            /* Verify metadata for the 1st column (id_user) */
            hc = hgrid_get_column(hg, 0);
            TEST_CHECK(!strcmp(hc->name, "id_user"));  /* Column name matches */
            TEST_CHECK(hc->size == 4);                  /* Column size is correct */

            /* Verify metadata for the 2nd column (name) */
            hc = hgrid_get_column(hg, 1);
            TEST_CHECK(!strcmp(hc->name, "name"));      /* Column name matches */
            TEST_CHECK(hc->size == 12);                 /* Column size is correct */

            /* ===== Data Manipulation (DML Operations) ===== */

            /* CREATE TABLE DATA - Initialize data grid with schema from header */
            g = dgrid_init(p, 8096, GT_FIXED, hg);

            /* INSERT ROW INTO TABLE - Add a new row and populate it */
            row = dgrid_alloc_row(g);    /* Allocate a new row slot */
            if (row) {
                Column c;

                /* Verify that one row is now occupied */
                TEST_CHECK(g->occupied == 1);

                /* Insert data into the "id_user" column (column index 0) */
                c = dgrid_get_column(hg, g, 0, 0);
                strncpy(c, "123", hgrid_get_column(hg, 0)->size);

                /* Insert data into the "name" column (column index 1) */
                c = dgrid_get_column(hg, g, 0, 1);
                strncpy(c, "12345678", hgrid_get_column(hg, 1)->size);

                /* Verify the data we just inserted in "id_user" column */
                c = dgrid_get_column(hg, g, 0, 0);
                TEST_CHECK(!strcmp(c, "123"));

                /* Verify the data we just inserted in "name" column */
                c = dgrid_get_column(hg, g, 0, 1);
                TEST_CHECK(!strcmp(c, "12345678"));
            } else
                TEST_FAIL();  /* Row allocation failed - grid might be full */

            /* Clean up allocated memory */
            free(hp);
            free(p);
        }

    TEST_SUITE_END()  /* End of grid test suite */

    TEST_SUITE(ilist2)

        TEST_CASE(back) {
            int  *val = ilist2_create(val, 16);

            TEST_CHECK(ilist2_empty(val));
            TEST_CHECK(ilist2_put_back(val, 5) != ILIST2_UNDEF);
            TEST_CHECK(!ilist2_empty(val));
            TEST_CHECK(ilist2_get_back(val) == 5);
            TEST_CHECK(ilist2_put_back(val, 6) != ILIST2_UNDEF);
            TEST_CHECK(ilist2_get_back(val) == 6);
            TEST_CHECK(ilist2_pop_back(val) == 6);
            TEST_CHECK(ilist2_get_back(val) == 5);
            TEST_CHECK(ilist2_pop_back(val) == 5);
            TEST_CHECK(ilist2_empty(val));

            ilist2_free(val);
        }

        TEST_CASE(front) {
            int  *val = ilist2_create(val, 16);

            TEST_CHECK(ilist2_empty(val));
            TEST_CHECK(ilist2_put_front(val, 5) != ILIST2_UNDEF);
            TEST_CHECK(!ilist2_empty(val));
            TEST_CHECK(ilist2_get_front(val) == 5);
            TEST_CHECK(ilist2_put_front(val, 6) != ILIST2_UNDEF);
            TEST_CHECK(ilist2_get_front(val) == 6);
            TEST_CHECK(ilist2_pop_front(val) == 6);
            TEST_CHECK(ilist2_get_front(val) == 5);
            TEST_CHECK(ilist2_pop_front(val) == 5);
            TEST_CHECK(ilist2_empty(val));

            ilist2_free(val);
        }

    TEST_SUITE_END()    /* End of ilist2 test suite */

    TEST_SUITE(ihash)

        /* Ordinary node */
        typedef struct hentry {
            ssize_t key;
            size_t value;
        } hentry;

        /* Node with member */
        typedef struct mentry {
            ssize_t key;
            int my_member;
        } mentry;

        /* User defined hash function */
        size_t my_hash_fn(size_t key) {
            return key * 31;
        }

        TEST_CASE(basic) {
            /* Create hash */
            hentry  *he = ihash_create(he, 5, 6, NULL);
            ihash *hash = (ihash *)he;
            hentry *tmp;

            /* Test inner structure */
            TEST_CHECK(hash->bucketsz == 5);
            TEST_CHECK(hash->chainsz == 6);

            /* Put a new node into hash */
            tmp = ihash_put(he, 5, 9);
            TEST_CHECK(tmp);
            TEST_CHECK(tmp->key == 5);
            TEST_CHECK(tmp->value == 9);

            /* Get a node that doesn't exist */
            tmp = ihash_get(he, 4);
            TEST_CHECK(tmp == NULL);

            /* Get the previously inserted node */
            tmp = ihash_get(he, 5);
            TEST_CHECK(tmp);
            TEST_CHECK(tmp->key == 5);
            TEST_CHECK(tmp->value == 9);

            ihash_free(he);
        }

        TEST_CASE(ihash_init) {
            size_t size = ihash_get_required_memory_size(16, 32, sizeof(hentry));
            hentry *hash = malloc(size), *tmp;
            hash = ihash_init(hash, hash, 16, 32, NULL);

            ihash_put(hash, 10, 100);
            tmp = ihash_get(hash, 10);

            TEST_CHECK(tmp->key == 10);
            TEST_CHECK(tmp->value == 100);

            free(hash);
        }

        TEST_CASE(ihash_erase) {
            hentry *hash = ihash_create(hash, 4, 8, NULL);

            /* Insert some values */
            ihash_put(hash, 1, 100);
            ihash_put(hash, 2, 200);
            ihash_put(hash, 3, 300);
            ihash_put(hash, 4, 400);
            ihash_put(hash, 5, 500);
            ihash_put(hash, 11, 700);
            ihash_put(hash, 7, 200);
            ihash_erase(hash, 5);

            TEST_CHECK(ihash_erase(hash, 2));   /* erase existent node */
            TEST_CHECK(!ihash_erase(hash, 25)); /* erase non-existent node */
            TEST_CHECK(!ihash_erase(hash, 2));  /* erase erased node */
            TEST_CHECK(ihash_erase(hash, 11));  /* erase existent node */

            ihash_free(hash);
        }

        TEST_CASE(ihash_foreach) {
            hentry *hash = ihash_create(hash, 4, 8, NULL), *tmp;

            ihash_put(hash, 1, 100);
            ihash_put(hash, 2, 200);
            ihash_put(hash, 30, 300);
            ihash_put(hash, 40, 400);
            ihash_put(hash, 1, 150);
            ihash_put(hash, 6, 500);

            ihash_foreach(tmp, hash) {
                /* @todo make this test work */
                /* printf("%lu: %lu\n", tmp->key, tmp->value); */
            }

            ihash_free(hash);
        }

        TEST_CASE(ihash_collision) {
            hentry *hash = ihash_create(hash, 4, 8, NULL);

            /* Keys that hash to same bucket (assuming identity hash) */
            ihash_put(hash, 4, 400);   /* bucket 0 */
            ihash_put(hash, 8, 800);   /* bucket 0 - collision */
            ihash_put(hash, 12, 1200); /* bucket 0 - collision */

            TEST_CHECK(ihash_get(hash, 4)->value == 400);
            TEST_CHECK(ihash_get(hash, 8)->value == 800);
            TEST_CHECK(ihash_get(hash, 12)->value == 1200);

            /* Update value in chain */
            ihash_put(hash, 8, 888);
            TEST_CHECK(ihash_get(hash, 8)->value == 888);

            ihash_free(hash);
        }

        TEST_CASE(ihash_collision_erase_chain_head) {
            hentry *hash = ihash_create(hash, 4, 8, NULL);

            ihash_put(hash, 4, 400);   /* bucket 0 */
            ihash_put(hash, 8, 800);   /* bucket 0 - chain node */
            ihash_put(hash, 12, 1200); /* bucket 0 - chain node */

            /* Erase head of chain (bucket node) */
            ihash_erase(hash, 4);

            TEST_CHECK(ihash_get(hash, 4) == NULL);
            TEST_CHECK(ihash_get(hash, 8)->value == 800);
            TEST_CHECK(ihash_get(hash, 12)->value == 1200);

            ihash_free(hash);
        }

        TEST_CASE(ihash_collision_erase_middle_chain) {
            hentry *hash = ihash_create(hash, 4, 8, NULL);

            ihash_put(hash, 4, 400);   /* bucket 0 */
            ihash_put(hash, 8, 800);   /* bucket 0 - chain node */
            ihash_put(hash, 12, 1200); /* bucket 0 - chain node */

            /* Erase middle of chain */
            ihash_erase(hash, 8);

            TEST_CHECK(ihash_get(hash, 4)->value == 400);
            TEST_CHECK(ihash_get(hash, 8) == NULL);
            TEST_CHECK(ihash_get(hash, 12)->value == 1200);

            ihash_free(hash);
        }

        TEST_CASE(ihash_full) {
            hentry *hash = ihash_create(hash, 2, 2, NULL); /* Small table */

            /* Fill all slots */
            TEST_CHECK(ihash_put(hash, 1, 100) != NULL);
            TEST_CHECK(ihash_put(hash, 2, 200) != NULL);
            TEST_CHECK(ihash_put(hash, 3, 300) != NULL);
            TEST_CHECK(ihash_put(hash, 4, 400) != NULL);

            /* Should fail when full */
            TEST_CHECK(ihash_put(hash, 5, 500) == NULL);

            /* But existing keys should update */
            TEST_CHECK(ihash_put(hash, 2, 250) != NULL);
            TEST_CHECK(ihash_get(hash, 2)->value == 250);

            ihash_free(hash);
        }

        TEST_CASE(ihash_clear) {
            hentry *hash = ihash_create(hash, 4, 8, NULL);

            ihash_put(hash, 1, 100);
            ihash_put(hash, 2, 200);
            ihash_put(hash, 3, 300);

            ihash_clear(hash, NULL);

            TEST_CHECK(ihash_get(hash, 1) == NULL);
            TEST_CHECK(ihash_get(hash, 2) == NULL);
            TEST_CHECK(ihash_get(hash, 3) == NULL);

            /* Should work after clear */
            ihash_put(hash, 10, 1000);
            TEST_CHECK(ihash_get(hash, 10)->value == 1000);

            ihash_free(hash);
        }

        TEST_CASE(ihash_put_struct) {
            hentry *hash = ihash_create(hash, 4, 8, NULL);
            hentry data1 = {42, 100};
            hentry data2 = {43, 200};

            ihash_put_struct(hash, &data1);
            ihash_put_struct(hash, &data2);

            TEST_CHECK(ihash_get(hash, 42)->value == 100);
            TEST_CHECK(ihash_get(hash, 43)->value == 200);

            /* Update with struct */
            hentry data1_new = {42, 999};
            ihash_put_struct(hash, &data1_new);
            TEST_CHECK(ihash_get(hash, 42)->value == 999);

            ihash_free(hash);
        }

        TEST_CASE(ihash_put_key) {
            /* For set-like behavior without value field */
            struct set_entry {
                ssize_t key;
                /* no value field */
            };

            struct set_entry *set = ihash_create(set, 4, 8, NULL);

            ihash_put_key(set, 100);
            ihash_put_key(set, 200);
            ihash_put_key(set, 300);

            TEST_CHECK(ihash_exists(set, 100));
            TEST_CHECK(ihash_exists(set, 200));
            TEST_CHECK(ihash_exists(set, 300));
            TEST_CHECK(!ihash_exists(set, 400));

            /* Duplicate insert should not fail */
            ihash_put_key(set, 100);
            TEST_CHECK(ihash_exists(set, 100));

            ihash_free(set);
        }

        TEST_CASE(ihash_get_member_ptr) {
            mentry *hash = ihash_create(hash, 4, 8, NULL);
            mentry e = { .key = 25, .my_member = 18 };

            ihash_put_struct(hash, &e);

            int *val = ihash_get_member_ptr(hash, 25, my_member);
            TEST_CHECK(val != NULL);
            TEST_CHECK(*val == 18);

            *val = 999;  /* Modify through pointer */
            TEST_CHECK(ihash_get(hash, 25)->my_member == 999);

            /* Non-existent key */
            TEST_CHECK(ihash_get_member_ptr(hash, 999, my_member) == NULL);

            ihash_free(hash);
        }

        TEST_CASE(ihash_get_member) {
            mentry *hash = ihash_create(hash, 4, 8, NULL);
            mentry e = { .key = 25, .my_member = -100 };

            ihash_put_struct(hash, &e);

            int val = ihash_get_member(hash, 25, my_member);
            TEST_CHECK(val == -100);

            /* Non-existent key returns zero-initialized */
            val = ihash_get_member(hash, 999, my_member);
            TEST_CHECK(val == 0);

            ihash_free(hash);
        }

        TEST_CASE(ihash_get_value_ptr) {
            hentry *hash = ihash_create(hash, 4, 8, NULL);
            ihash_put(hash, 42, 100);

            size_t *val = ihash_get_value_ptr(hash, 42);
            TEST_CHECK(val != NULL);
            TEST_CHECK(*val == 100);

            ihash_free(hash);
        }

        TEST_CASE(ihash_get_value) {
            hentry *hash = ihash_create(hash, 4, 8, NULL);
            ihash_put(hash, 42, 100);

            size_t val = ihash_get_value(hash, 42);
            TEST_CHECK(val == 100);

            ihash_free(hash);
        }

        TEST_CASE(ihash_zero_chains) {
            hentry *hash = ihash_create(hash, 4, 0, NULL); /* No overflow nodes */

            /* Fill all buckets */
            ihash_put(hash, 1, 100);
            ihash_put(hash, 2, 200);
            ihash_put(hash, 3, 300);
            ihash_put(hash, 4, 400);

            /* Collision should fail (no chain nodes) */
            TEST_CHECK(ihash_put(hash, 5, 500) == NULL);

            /* Existing keys still work */
            TEST_CHECK(ihash_get(hash, 2)->value == 200);

            ihash_free(hash);
        }

        TEST_CASE(ihash_zero_buckets) {
            hentry *hash = ihash_create(hash, 0, 8, NULL); /* Should handle? */

            TEST_CHECK(!hash);  /* Error, impossible to init a hash without buckets */
        }

        TEST_CASE(ihash_reuse_freelist) {
            hentry *hash = ihash_create(hash, 2, 2, NULL);

            /* Fill */
            ihash_put(hash, 1, 100);
            ihash_put(hash, 2, 200);
            ihash_put(hash, 3, 300); /* Uses chain */
            ihash_put(hash, 4, 400); /* Uses chain */

            /* Erase some */
            ihash_erase(hash, 2);
            ihash_erase(hash, 3);

            /* New insert should reuse freed nodes */
            TEST_CHECK(ihash_put(hash, 5, 500) != NULL);
            TEST_CHECK(ihash_put(hash, 6, 600) != NULL);

            ihash_free(hash);
        }

        TEST_CASE(ihash_erase_all) {
            hentry *hash = ihash_create(hash, 4, 8, NULL);

            /* Hash can contain 12 nodes only */
            for (size_t i = 0; i < 20; ++i) {
                hentry *result = ihash_put(hash, i, i * 100);
                TEST_CHECK(i < 12 ? result != NULL : result == NULL);
            }

            /* Erase all */
            for (size_t i = 0; i < 12; ++i) {
                int result = ihash_erase(hash, i);
                TEST_CHECK(i < 12 ? result : !result);
            }

            /* Verify all gone */
            for (size_t i = 0; i < 20; ++i) {
                TEST_CHECK(ihash_get(hash, i) == NULL);
            }

            /* Should be able to insert again */
            for (size_t i = 0; i < 10; ++i) {
                TEST_CHECK(ihash_put(hash, i, i * 100) != NULL);
            }

            /* Verify all gone */
            for (size_t i = 0; i < 10; ++i) {
                TEST_CHECK(ihash_get(hash, i));
            }

            ihash_free(hash);
        }

        TEST_CASE(ihash_custom_hash) {
            hentry *hash = ihash_create(hash, 4, 8, my_hash_fn);
            TEST_CHECK(hash != NULL);

            /* Should work same way */
            ihash_put(hash, 1, 100);
            ihash_put(hash, 2, 200);

            TEST_CHECK(ihash_get(hash, 1)->value == 100);
            TEST_CHECK(ihash_get(hash, 2)->value == 200);

            ihash_free(hash);
        }

    TEST_SUITE_END()  /* End of ihash test suite */

TEST_END()  /* End of basic test unit */

