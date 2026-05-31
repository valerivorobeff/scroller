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
            Grid *hg = hgrid_init(hp, 8096, GT_FIXED), *g;
            HColumn *hc;
            Row row;

            /* CREATE TABLE HEADER */
            /* ALTER TABLE ADD COLUMN */
            if (!hgrid_add_column(hg, "id_user", 4)) {
                TEST_FAIL();
            }
            TEST_CHECK(hg->occupied == 1);

            /* ALTER TABLE ADD COLUMN */
            if (!hgrid_add_column(hg, "name", 12)) {
                TEST_FAIL();
            }
            TEST_CHECK(hg->occupied == 2);

            /* Check the 1st row of hgrid */
            hc = hgrid_get_column(hg, 0);
            TEST_CHECK(!strcmp(hc->name, "id_user"));
            TEST_CHECK(hc->size == 4);

            /* Check the 2nd row of hgrid */
            hc = hgrid_get_column(hg, 1);
            TEST_CHECK(!strcmp(hc->name, "name"));
            TEST_CHECK(hc->size == 12);

            /* CREATE TABLE DATA */
            g = dgrid_init(p, 8096, GT_FIXED, hg);

            /* INSERT ROW INTO TABLE */
            row = dgrid_alloc_row(g);    /* alloc row */
            if (row) {
                Column c;

                TEST_CHECK(g->occupied == 1);

                c = dgrid_get_column(hg, g, 0, 0);
                strncpy(c, "123", hgrid_get_column(hg, 0)->size);

                c = dgrid_get_column(hg, g, 0, 1);
                strncpy(c, "12345678", hgrid_get_column(hg, 1)->size);

                /* Check data we have just inserted */
                c = dgrid_get_column(hg, g, 0, 0);
                TEST_CHECK(!strcmp(c, "123"));

                c = dgrid_get_column(hg, g, 0, 1);
                TEST_CHECK(!strcmp(c, "12345678"));
            } else
                TEST_FAIL();

            free(hp);
            free(p);
        }
 
    TEST_SUITE_END()

TEST_END()

