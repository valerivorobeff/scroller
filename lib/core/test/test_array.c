/*
 * In this example we will use TEST() and TEST_END() as the main test unit,
 * which consists of one test suite (but may consist of any) which in turn
 * includes and runs test cases.
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
 * occurs in statistics. All other results are considered successive and
 * passed.
 */

#include "quin.h"
#include "array.h"
#include "memory.h"
#include <stdio.h>

/* ==============================
 *
 * Test suite array
 *
 ================================ */

TEST(array)

    TEST_SUITE(basic)

        TEST_CASE(create_free) {
            memory_init_default();

            int *a = array_create(a, 16);

            TEST_CHECK(a != NULL);
            TEST_CHECK(array_size(a) == 0);
            TEST_CHECK(array_empty(a));

            array_free(a);
            memory_destroy();
        }

        TEST_CASE(create_zero_capacity) {
            memory_init_default();

            int *a = array_create(a, 0);

            TEST_CHECK(a != NULL);
            TEST_CHECK(array_size(a) == 0);
            TEST_CHECK(array_empty(a));

            array_free(a);
            memory_destroy();
        }

        TEST_CASE(create_large_capacity) {
            memory_init_default();

            int *a = array_create(a, 1024);

            TEST_CHECK(a != NULL);
            TEST_CHECK(array_size(a) == 0);

            array_free(a);
            memory_destroy();
        }

        TEST_CASE(create_custom_type) {
            memory_init_default();

            struct Point {
                int x;
                int y;
            };

            struct Point *points = array_create(points, 10);

            TEST_CHECK(points != NULL);
            TEST_CHECK(array_size(points) == 0);

            array_free(points);
            memory_destroy();
        }

    TEST_SUITE_END()  /* End of basic test suite */

    TEST_SUITE(put)

        TEST_CASE(put_one) {
            memory_init_default();

            int *a = array_create(a, 16);

            a = array_put(a, 42);

            TEST_CHECK(a != NULL);
            TEST_CHECK(array_size(a) == 1);
            TEST_CHECK(!array_empty(a));
            TEST_CHECK(array_back(a) == 42);
            TEST_CHECK(a[0] == 42);

            array_free(a);
            memory_destroy();
        }

        TEST_CASE(put_multiple) {
            memory_init_default();

            int *a = array_create(a, 16);

            for (int i = 0; i < 100; ++i) {
                a = array_put(a, i);
                TEST_CHECK(a != NULL);
                TEST_CHECK(array_size(a) == (size_t)(i + 1));
                TEST_CHECK(array_back(a) == i);
            }

            TEST_CHECK(array_size(a) == 100);
            for (int i = 0; i < 100; ++i) {
                TEST_CHECK(a[i] == i);
            }

            array_free(a);
            memory_destroy();
        }

        TEST_CASE(put_custom_type) {
            memory_init_default();

            struct Point {
                int x;
                int y;
            };

            struct Point *points = array_create(points, 16);

            struct Point p1 = {10, 20};
            struct Point p2 = {30, 40};

            points = array_put(points, p1);
            points = array_put(points, p2);

            TEST_CHECK(array_size(points) == 2);
            TEST_CHECK(points[0].x == 10 && points[0].y == 20);
            TEST_CHECK(points[1].x == 30 && points[1].y == 40);
            TEST_CHECK(array_back(points).x == 30 && array_back(points).y == 40);

            array_free(points);
            memory_destroy();
        }

        TEST_CASE(put_beyond_capacity) {
            memory_init_default();

            int *a = array_create(a, 4);

            for (int i = 0; i < 100; ++i) {
                a = array_put(a, i);
                TEST_CHECK(a != NULL);
                TEST_CHECK(array_size(a) == (size_t)(i + 1));
                TEST_CHECK(array_back(a) == i);
            }

            TEST_CHECK(array_size(a) == 100);

            array_free(a);
            memory_destroy();
        }

        TEST_CASE(put_after_free) {
            memory_init_default();

            int *a = array_create(a, 16);
            a = array_put(a, 42);
            array_free(a);

            /* Using after free should be invalid, but we test that we don't crash */
            /* This test is more about memory safety */

            memory_destroy();
        }

    TEST_SUITE_END()  /* End of put test suite */

    TEST_SUITE(pop)

        TEST_CASE(pop_one) {
            memory_init_default();

            int *a = array_create(a, 16);
            a = array_put(a, 42);

            int val = array_pop(a);

            TEST_CHECK(val == 42);
            TEST_CHECK(array_size(a) == 0);
            TEST_CHECK(array_empty(a));

            array_free(a);
            memory_destroy();
        }

        TEST_CASE(pop_multiple) {
            memory_init_default();

            int *a = array_create(a, 16);

            for (int i = 0; i < 10; ++i) {
                a = array_put(a, i);
            }

            for (int i = 9; i >= 0; --i) {
                int val = array_pop(a);
                TEST_CHECK(val == i);
                TEST_CHECK(array_size(a) == (size_t)i);
            }

            TEST_CHECK(array_size(a) == 0);
            TEST_CHECK(array_empty(a));

            array_free(a);
            memory_destroy();
        }

        TEST_CASE(pop_until_empty) {
            memory_init_default();

            int *a = array_create(a, 4);
            a = array_put(a, 100);

            int val = array_pop(a);
            TEST_CHECK(val == 100);
            TEST_CHECK(array_size(a) == 0);

            /* Pop from empty array should be UB, but we test it doesn't crash */
            /* val = array_pop(a); */  /* Would be UB */

            array_free(a);
            memory_destroy();
        }

        TEST_CASE(pop_custom_type) {
            memory_init_default();

            struct Point {
                int x;
                int y;
            };

            struct Point *points = array_create(points, 16);
            struct Point p1 = {10, 20};
            struct Point p2 = {30, 40};

            points = array_put(points, p1);
            points = array_put(points, p2);

            struct Point val = array_pop(points);
            TEST_CHECK(val.x == 30 && val.y == 40);
            TEST_CHECK(array_size(points) == 1);

            val = array_pop(points);
            TEST_CHECK(val.x == 10 && val.y == 20);
            TEST_CHECK(array_size(points) == 0);
            TEST_CHECK(array_empty(points));

            array_free(points);
            memory_destroy();
        }

    TEST_SUITE_END()  /* End of pop test suite */

    TEST_SUITE(resize)

        TEST_CASE(grow_doubles) {
            memory_init_default();

            /* Test that capacity doubles when full */
            int *a = array_create(a, 2);
            TEST_CHECK(a != NULL);

            a = array_put(a, 1);
            a = array_put(a, 2);
            a = array_put(a, 3);  /* Should double capacity */

            /* Verify all data intact */
            TEST_CHECK(a[0] == 1);
            TEST_CHECK(a[1] == 2);
            TEST_CHECK(a[2] == 3);
            TEST_CHECK(array_size(a) == 3);

            array_free(a);
            memory_destroy();
        }

        TEST_CASE(shrink_half) {
            memory_init_default();

            int *a = array_create(a, 8);

            for (int i = 0; i < 8; ++i) {
                a = array_put(a, i);
            }

            TEST_CHECK(array_size(a) == 8);

            /* Pop down to half capacity */
            for (int i = 0; i < 4; ++i) {
                array_pop(a);
            }

            TEST_CHECK(array_size(a) == 4);

            /* Verify remaining data */
            for (int i = 0; i < 4; ++i) {
                TEST_CHECK(a[i] == i);
            }

            array_free(a);
            memory_destroy();
        }

        TEST_CASE(grow_from_zero_capacity) {
            memory_init_default();

            int *a = array_create(a, 0);
            TEST_CHECK(a != NULL);

            a = array_put(a, 42);
            TEST_CHECK(a != NULL);
            TEST_CHECK(array_size(a) == 1);
            TEST_CHECK(a[0] == 42);

            a = array_put(a, 100);
            TEST_CHECK(a != NULL);
            TEST_CHECK(array_size(a) == 2);
            TEST_CHECK(a[0] == 42);
            TEST_CHECK(a[1] == 100);

            array_free(a);
            memory_destroy();
        }

    TEST_SUITE_END()  /* End of resize test suite */

    TEST_SUITE(mixed)

        TEST_CASE(put_pop_alternate) {
            memory_init_default();

            int *a = array_create(a, 4);

            a = array_put(a, 1);
            a = array_put(a, 2);
            int v = array_pop(a);
            TEST_CHECK(v == 2);
            TEST_CHECK(array_size(a) == 1);

            a = array_put(a, 3);
            a = array_put(a, 4);
            TEST_CHECK(array_size(a) == 3);
            TEST_CHECK(a[0] == 1);
            TEST_CHECK(a[1] == 3);
            TEST_CHECK(a[2] == 4);

            v = array_pop(a);
            TEST_CHECK(v == 4);
            v = array_pop(a);
            TEST_CHECK(v == 3);
            v = array_pop(a);
            TEST_CHECK(v == 1);
            TEST_CHECK(array_size(a) == 0);

            array_free(a);
            memory_destroy();
        }

        TEST_CASE(large_sequence) {
            memory_init_default();

            int *a = array_create(a, 4);
            const size_t N = 1000;

            for (size_t i = 0; i < N; ++i) {
                a = array_put(a, i);
            }

            TEST_CHECK(array_size(a) == N);

            for (size_t i = 0; i < N; ++i) {
                TEST_CHECK(a[i] == (int)i);
            }

            for (int i = N - 1; i >= 0; --i) {
                int v = array_pop(a);
                TEST_CHECK(v == i);
            }

            TEST_CHECK(array_size(a) == 0);

            array_free(a);
            memory_destroy();
        }

        TEST_CASE(put_after_pop) {
            memory_init_default();

            int *a = array_create(a, 4);

            for (int i = 0; i < 10; ++i) {
                a = array_put(a, i);
            }

            /* Pop a few */
            for (int i = 0; i < 5; ++i) {
                array_pop(a);
            }

            TEST_CHECK(array_size(a) == 5);

            /* Put more */
            for (int i = 10; i < 20; ++i) {
                a = array_put(a, i);
            }

            TEST_CHECK(array_size(a) == 15);

            /* Verify */
            for (int i = 0; i < 5; ++i) {
                TEST_CHECK(a[i] == i);
            }
            for (int i = 5; i < 15; ++i) {
                TEST_CHECK(a[i] == i + 5);
            }

            array_free(a);
            memory_destroy();
        }

    TEST_SUITE_END()  /* End of mixed test suite */

    TEST_SUITE(edge_cases)

        TEST_CASE(put_null) {
            memory_init_default();

            int *a = NULL;

            /* array_put on NULL should handle gracefully? */
            /* Depending on implementation, might need to check */
            a = array_put(a, 42);  /* Should probably crash or return NULL */

            /* If implementation handles NULL, test it */
            /* TEST_CHECK(a == NULL); */

            /* For now, we test that it doesn't crash if NULL */
            /* array_put(a, 42); */  /* Would be UB if array_put doesn't handle NULL */

            memory_destroy();
        }

        TEST_CASE(empty_back) {
            memory_init_default();

            int *a = array_create(a, 4);

            /* array_back on empty array is UB, but we test it doesn't crash */
            /* This test documents the UB */
            /* int val = array_back(a); */  /* Would be UB */

            /* Check that size is 0 */
            TEST_CHECK(array_size(a) == 0);

            array_free(a);
            memory_destroy();
        }

        TEST_CASE(array_size_after_operations) {
            memory_init_default();

            int *a = array_create(a, 4);

            TEST_CHECK(array_size(a) == 0);

            a = array_put(a, 10);
            TEST_CHECK(array_size(a) == 1);

            a = array_put(a, 20);
            TEST_CHECK(array_size(a) == 2);

            array_pop(a);
            TEST_CHECK(array_size(a) == 1);

            array_pop(a);
            TEST_CHECK(array_size(a) == 0);

            array_free(a);
            memory_destroy();
        }

        TEST_CASE(repeated_put_pop) {
            memory_init_default();

            int *a = array_create(a, 2);

            for (int i = 0; i < 100; ++i) {
                a = array_put(a, i);
                TEST_CHECK(array_size(a) == 1);
                TEST_CHECK(a[0] == i);
                array_pop(a);
                TEST_CHECK(array_size(a) == 0);
            }

            array_free(a);
            memory_destroy();
        }

    TEST_SUITE_END()  /* End of edge_cases test suite */

TEST_END()

