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

    TEST_SUITE(ihash)

        typedef struct hentry {
            ssize_t key;
            int value;
            ssize_t next;
        } hentry;

        TEST_CASE(basic) {
            /* Create hash */
            hentry  *he = ihash_create(he, 5, 6);
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

        TEST_CASE(ihash_erase) {
            hentry *hash = ihash_create(hash, 4, 8);

            /* Insert some values */
            ihash_put(hash, 10, 100);
            ihash_put(hash, 20, 200);
            ihash_put(hash, 30, 300);
            ihash_put(hash, 40, 400);

            TEST_CHECK(ihash_erase(hash, 20));  /* erase existent node */
            TEST_CHECK(!ihash_erase(hash, 25)); /* erase non-existent node */
            TEST_CHECK(!ihash_erase(hash, 20)); /* erase erased node */
        }

    TEST_SUITE_END()  /* End of ihash test suite */

TEST_END()  /* End of basic test unit */

