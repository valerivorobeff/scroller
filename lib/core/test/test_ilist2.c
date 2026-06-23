/**
 * @file test_ilist2.c
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
#include "ilist2.h"
#include <malloc.h>

TEST(ilist2)

    TEST_SUITE(ilist2)

        TEST_CASE(back) {
            int  *val = ilist2_create(val, 16);

            TEST_CHECK(ilist2_empty(val));
            TEST_CHECK(ilist2_put_back(val, 5) == 0);
            TEST_CHECK(!ilist2_empty(val));
            TEST_CHECK(ilist2_get_back(val) == 5);
            TEST_CHECK(ilist2_put_back(val, 6) == 1);
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
            TEST_CHECK(ilist2_put_front(val, 5) == 0);
            TEST_CHECK(!ilist2_empty(val));
            TEST_CHECK(ilist2_get_front(val) == 5);
            TEST_CHECK(ilist2_put_front(val, 6) == 1);
            TEST_CHECK(ilist2_get_front(val) == 6);
            TEST_CHECK(ilist2_pop_front(val) == 6);
            TEST_CHECK(ilist2_get_front(val) == 5);
            TEST_CHECK(ilist2_pop_front(val) == 5);
            TEST_CHECK(ilist2_empty(val));

            ilist2_free(val);
        }

        TEST_CASE(move) {
            int  *val = ilist2_create(val, 16);

            ilist2_idx_t idx = ilist2_put_back(val, 5);
            ilist2_put_front(val, 6);
            ilist2_put_back(val, 7);

            TEST_CHECK(ilist2_get_back(val) == 7);
            ilist2_move_back_by_idx(val, idx);

            TEST_CHECK(ilist2_get_front(val) == 6);
            TEST_CHECK(ilist2_get_back(val) == 5);

            ilist2_move_front_by_idx(val, idx);

            TEST_CHECK(ilist2_get_front(val) == 5);
            TEST_CHECK(ilist2_get_back(val) == 7);

            ilist2_move_back_by_idx(val, idx);

            TEST_CHECK(ilist2_get_front(val) == 6);
            TEST_CHECK(ilist2_get_back(val) == 5);

            ilist2_free(val);
        }

        TEST_CASE(fill_and_empty) {
            int *val = ilist2_create(val, 4);

            /* Fill the list */
            for (size_t i = 0; i < 4; ++i) {
                TEST_CHECK(ilist2_put_back(val, i) == (ilist2_idx_t)i);
            }

            TEST_CHECK(!ilist2_empty(val));

            /* Try to add beyond capacity */
            TEST_CHECK(ilist2_put_back(val, 100) == ILIST2_UNDEF);
            TEST_CHECK(ilist2_put_front(val, 200) == ILIST2_UNDEF);

            /* Empty the list from back */
            for (size_t i = 3; i > 0; --i) {
                TEST_CHECK(ilist2_pop_back(val) == (int)i);
            }

            TEST_CHECK(ilist2_pop_back(val) == 0);
            TEST_CHECK(ilist2_empty(val));

            /* Should be able to add again (freelist reused) */
            TEST_CHECK(ilist2_put_back(val, 42) == 0);
            TEST_CHECK(ilist2_get_back(val) == 42);

            ilist2_free(val);
        }

        TEST_CASE(mixed_push_pop) {
            int *val = ilist2_create(val, 8);

            ilist2_put_back(val, 1);
            ilist2_put_front(val, 2);
            ilist2_put_back(val, 3);
            ilist2_put_front(val, 4);

            /* List should be: 4, 2, 1, 3 */
            TEST_CHECK(ilist2_get_front(val) == 4);
            TEST_CHECK(ilist2_get_back(val) == 3);

            TEST_CHECK(ilist2_pop_front(val) == 4);
            TEST_CHECK(ilist2_pop_back(val) == 3);
            TEST_CHECK(ilist2_get_front(val) == 2);
            TEST_CHECK(ilist2_get_back(val) == 1);

            TEST_CHECK(ilist2_pop_front(val) == 2);
            TEST_CHECK(ilist2_pop_back(val) == 1);
            TEST_CHECK(ilist2_empty(val));

            ilist2_free(val);
        }

        TEST_CASE(move_complex) {
            int *val = ilist2_create(val, 8);

            /* Create list: 1, 2, 3, 4, 5 */
            ilist2_idx_t idx1 = ilist2_put_back(val, 1);
            ilist2_idx_t idx2 = ilist2_put_back(val, 2);
            ilist2_idx_t idx3 = ilist2_put_back(val, 3);
            ilist2_put_back(val, 4);
            ilist2_put_back(val, 5);

            TEST_CHECK(ilist2_get_front(val) == 1);
            TEST_CHECK(ilist2_get_back(val) == 5);

            /* Move middle element (3) to back: 1, 2, 4, 5, 3 */
            ilist2_move_back_by_idx(val, idx3);
            TEST_CHECK(ilist2_get_back(val) == 3);
            TEST_CHECK(ilist2_get_front(val) == 1);

            /* Move front element (1) to back: 2, 4, 5, 3, 1 */
            ilist2_move_back_by_idx(val, idx1);
            TEST_CHECK(ilist2_get_back(val) == 1);
            TEST_CHECK(ilist2_get_front(val) == 2);

            /* Move back element (1) to front: 1, 2, 4, 5, 3 */
            ilist2_move_front_by_idx(val, idx1);
            TEST_CHECK(ilist2_get_front(val) == 1);
            TEST_CHECK(ilist2_get_back(val) == 3);

            /* Move middle to front: 2, 1, 4, 5, 3 */
            ilist2_move_front_by_idx(val, idx2);
            TEST_CHECK(ilist2_get_front(val) == 2);

            ilist2_free(val);
        }

        TEST_CASE(move_single_element) {
            int *val = ilist2_create(val, 8);

            ilist2_idx_t idx = ilist2_put_back(val, 42);
            TEST_CHECK(ilist2_get_front(val) == 42);
            TEST_CHECK(ilist2_get_back(val) == 42);

            /* Moving the only element should not break list */
            ilist2_move_back_by_idx(val, idx);
            TEST_CHECK(ilist2_get_front(val) == 42);
            TEST_CHECK(ilist2_get_back(val) == 42);
            TEST_CHECK(!ilist2_empty(val));

            ilist2_move_front_by_idx(val, idx);
            TEST_CHECK(ilist2_get_front(val) == 42);
            TEST_CHECK(ilist2_get_back(val) == 42);

            ilist2_free(val);
        }

        TEST_CASE(move_two_elements) {
            int *val = ilist2_create(val, 8);

            ilist2_idx_t idx1 = ilist2_put_back(val, 1);
            ilist2_put_back(val, 2);

            TEST_CHECK(ilist2_get_front(val) == 1);
            TEST_CHECK(ilist2_get_back(val) == 2);

            /* Move front to back: 2, 1 */
            ilist2_move_back_by_idx(val, idx1);
            TEST_CHECK(ilist2_get_front(val) == 2);
            TEST_CHECK(ilist2_get_back(val) == 1);

            /* Move back to front: 1, 2 */
            ilist2_move_front_by_idx(val, idx1);
            TEST_CHECK(ilist2_get_front(val) == 1);
            TEST_CHECK(ilist2_get_back(val) == 2);

            ilist2_free(val);
        }

        TEST_CASE(move_to_same_position) {
            int *val = ilist2_create(val, 8);

            ilist2_put_back(val, 1);
            ilist2_put_back(val, 2);
            ilist2_put_back(val, 3);

            /* Moving back node to back should do nothing */
            ilist2_move_back_by_idx(val, 2);  /* idx2 is back already? Actually idx2 is 1 */
            /* But idx2 is the second element (value 2), not back */

            TEST_CHECK(ilist2_get_back(val) == 3);

            /* Move front node to front - should be no-op */
            ilist2_move_front_by_idx(val, 0);  /* idx0 is value 1 */
            TEST_CHECK(ilist2_get_front(val) == 1);

            ilist2_free(val);
        }

        TEST_CASE(init_with_preallocated) {
            size_t size = ilist2_get_required_memory_size(16, sizeof(int));
            int *list = malloc(size);
            TEST_CHECK(list != NULL);

            list = ilist2_init(list, 16);
            TEST_CHECK(list != NULL);
            TEST_CHECK(ilist2_empty(list));

            ilist2_put_back(list, 100);
            TEST_CHECK(ilist2_get_back(list) == 100);

            ilist2_clear(list);
            TEST_CHECK(ilist2_empty(list));

            /* Should be able to use after clear */
            ilist2_put_back(list, 200);
            TEST_CHECK(ilist2_get_back(list) == 200);

            free(list);
        }

        TEST_CASE(zero_size) {
            int *val = ilist2_create(val, 0);
            TEST_CHECK(val != NULL);
            TEST_CHECK(ilist2_empty(val));

            /* Should not be able to add */
            TEST_CHECK(ilist2_put_back(val, 42) == ILIST2_UNDEF);
            TEST_CHECK(ilist2_put_front(val, 42) == ILIST2_UNDEF);
            TEST_CHECK(ilist2_empty(val));

            ilist2_free(val);
        }

        TEST_CASE(large_list) {
            int *val = ilist2_create(val, 100);

            /* Push many elements */
            for (size_t i = 0; i < 100; ++i) {
                TEST_CHECK(ilist2_put_back(val, i) == (ilist2_idx_t)i);
            }

            /* Verify front and back */
            TEST_CHECK(ilist2_get_front(val) == 0);
            TEST_CHECK(ilist2_get_back(val) == 99);

            /* Pop from back */
            for (size_t i = 99; i > 0; --i) {
                TEST_CHECK(ilist2_pop_back(val) == (int)i);
            }

            TEST_CHECK(ilist2_pop_back(val) == 0);
            TEST_CHECK(ilist2_empty(val));

            /* Push again (should reuse freelist) */
            for (size_t i = 0; i < 50; ++i) {
                TEST_CHECK(ilist2_put_back(val, i) == (ilist2_idx_t)i);
            }

            TEST_CHECK(ilist2_get_back(val) == 49);

            ilist2_free(val);
        }

        TEST_CASE(complex_put_pop_sequence) {
            int *val = ilist2_create(val, 10);

            /* Sequence: push back 1,2,3, push front 4,5, pop back, pop front */
            ilist2_put_back(val, 1);
            ilist2_put_back(val, 2);
            ilist2_put_back(val, 3);
            ilist2_put_front(val, 4);
            ilist2_put_front(val, 5);

            /* List: 5, 4, 1, 2, 3 */
            TEST_CHECK(ilist2_get_front(val) == 5);
            TEST_CHECK(ilist2_get_back(val) == 3);

            TEST_CHECK(ilist2_pop_back(val) == 3);
            TEST_CHECK(ilist2_pop_front(val) == 5);

            /* List: 4, 1, 2 */
            TEST_CHECK(ilist2_get_front(val) == 4);
            TEST_CHECK(ilist2_get_back(val) == 2);

            TEST_CHECK(ilist2_pop_back(val) == 2);
            TEST_CHECK(ilist2_pop_front(val) == 4);

            /* List: 1 */
            TEST_CHECK(ilist2_get_front(val) == 1);
            TEST_CHECK(ilist2_get_back(val) == 1);

            TEST_CHECK(ilist2_pop_back(val) == 1);
            TEST_CHECK(ilist2_empty(val));

            ilist2_free(val);
        }

    TEST_SUITE_END()    /* End of ilist2 test suite */

TEST_END()  /* End of ilist2 test unit */
