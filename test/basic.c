/*
 * In this example we will use TEST() and TEST_END() as the main test unit,
 * which conststs of one test suite (but may consist of any) which in turn
 * includes and runs 3 test cases.
 * Every test should start with TEST(name) and end with TEST_END() definition
 * where name defines the whole test name and is any allowed c identifier.
 * There can only be one TEST(name) - TEST_END() pair inside one test file.
 * Inside TEST(name) and TEST_END() there should be zero or more test suits.
 * Test suites begin with TEST_SUITE_BEGIN(name) and end with TEST_SUITE_END()
 * where name defines the test suite name and is any allowed c identifier.
 * Inside every test suite the should be zero or more test cases. Every test
 * case shoud begin with TEST_CASE(name) followed with a code block "{}" with
 * the test code inside and where name defines the test case name and is any
 * allowed c identifier.
 * Inside every test case there should be one or more TEST_REQUIRE(condition)
 * with the condition being any allowed c expression.
 * If the expression equals to 0, the test case is considered failed and
 * occures in statistics. All other results are considered successive and
 * passed.
 */

#include "quin.h"
#include "grid.h"
#include <malloc.h>
#include <string.h>

/* ==============================
 *
 * Test suite suite_basic
 *
 ================================ */

TEST(basic)

    TEST_SUITE(grid)

        TEST_CASE(test_1) {
            Page *hp = malloc(8096), *p = malloc(8096);
            Grid *hg = grid_init(hp, 8096, GT_FIXED, sizeof(HColumn)), *g;
            Row row;

            /* CREATE TABLE HEADER */
            /* ALTER TABLE ADD COLUMN */
            if (!hgrid_add_column(hg, "id_user", 4)) {

            }
            /* ALTER TABLE ADD COLUMN */
            if (!hgrid_add_column(hg, "name", 12)) {

            }

            /* CREATE TABLE DATA */
            g = grid_init(p, 8096, GT_FIXED, hgrid_get_row_size(hg));

            /* INSERT ROW INTO TABLE */
            row = grid_alloc_row(g);    /* alloc row */
            if (row) {
                HColumn *hc = grid_get_row(hg, 0);
                Column c = row + hc->offs;
                strncpy(c, "123", hc->size);

                hc = grid_get_row(hg, 1);
                c = row + hc->offs;
                strncpy(c, "12345678", hc->size);
            }

            TEST_REQUIRE(1);
        }
 
    TEST_SUITE_END()

TEST_END()

