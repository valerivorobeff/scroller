/**
 * @file test_sequence.c
 * @brief Unit tests for sequence functionality
 *
 * This file contains comprehensive unit tests for the sequence implementation,
 * covering initialization, value generation, cycling, and parameter updates.
 *
 * @author Based on Quin testing framework
 */

#include "quin.h"
#include "sequence.h"
#include <stdlib.h>

TEST(sequence)

    Grid *hsequence = NULL;
    Grid *sequence = NULL;

    TEST_SUITE(hsequence_init)

        TEST_CASE(hsequence_init) {
            hsequence = aligned_alloc(PAGESZ, PAGESZ);
            sequence = aligned_alloc(PAGESZ, PAGESZ);

            TEST_CHECK(hsequence_init(hsequence) == 0);
            TEST_CHECK(hsequence != NULL);
            TEST_CHECK(sequence != NULL);
        }

    TEST_SUITE_END()

    TEST_SUITE(sequence_init)

        TEST_CASE(init_valid) {
            /* Create sequence: 1 to 100, step 2, start 1, no cycle */
            int ret = sequence_init(hsequence, sequence, 1, 100, 1, 2, false);
            TEST_CHECK(ret == 0);
        }

        TEST_CASE(init_minval_ge_maxval) {
            int ret = sequence_init(hsequence, sequence, 100, 50, 75, 1, false);
            TEST_CHECK(ret != 0);
        }

        TEST_CASE(init_start_lt_minval) {
            int ret = sequence_init(hsequence, sequence, 1, 100, 0, 1, false);
            TEST_CHECK(ret != 0);
        }

        TEST_CASE(init_start_gt_maxval) {
            int ret = sequence_init(hsequence, sequence, 1, 100, 101, 1, false);
            TEST_CHECK(ret != 0);
        }

        TEST_CASE(init_increment_zero) {
            int ret = sequence_init(hsequence, sequence, 1, 100, 50, 0, false);
            TEST_CHECK(ret != 0);
        }

        TEST_CASE(init_negative_increment) {
            int ret = sequence_init(hsequence, sequence, 1, 100, 50, -2, false);
            TEST_CHECK(ret == 0);
        }

    TEST_SUITE_END()

    TEST_SUITE(sequence_currval)

        TEST_CASE(currval_before_nextval) {
            int64_t val;
            int ret;

            sequence_init(hsequence, sequence, 1, 100, 50, 1, false);

            ret = sequence_currval(hsequence, sequence, &val);
            TEST_CHECK(ret != 0);
        }

        TEST_CASE(currval_after_nextval) {
            int64_t val;

            sequence_init(hsequence, sequence, 1, 100, 50, 1, false);

            /* First call to nextval returns startval */
            TEST_CHECK(sequence_nextval(hsequence, sequence, &val) == 0);
            TEST_CHECK(val == 50);

            /* Now currval should work */
            TEST_CHECK(sequence_currval(hsequence, sequence, &val) == 0);
            TEST_CHECK(val == 50);
        }

        TEST_CASE(currval_after_multiple_nextval) {
            int64_t val;

            sequence_init(hsequence, sequence, 1, 100, 1, 2, false);

            /* Advance to 5 */
            sequence_nextval(hsequence, sequence, &val);
            sequence_nextval(hsequence, sequence, &val);
            sequence_nextval(hsequence, sequence, &val);

            TEST_CHECK(sequence_currval(hsequence, sequence, &val) == 0);
            TEST_CHECK(val == 5);
        }

    TEST_SUITE_END()

    TEST_SUITE(sequence_nextval)

        TEST_CASE(nextval_progressive) {
            int64_t val;

            sequence_init(hsequence, sequence, 1, 10, 1, 2, false);

            sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(val == 1);

            sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(val == 3);

            sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(val == 5);

            sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(val == 7);

            sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(val == 9);

            /* Exceed max, should fail */
            TEST_CHECK(sequence_nextval(hsequence, sequence, &val) != 0);
        }

        TEST_CASE(nextval_regressive) {
            int64_t val;

            sequence_init(hsequence, sequence, 1, 10, 9, -2, false);

            sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(val == 9);

            sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(val == 7);

            sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(val == 5);

            sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(val == 3);

            sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(val == 1);

            /* Exceed min, should fail */
            TEST_CHECK(sequence_nextval(hsequence, sequence, &val) != 0);
        }

        TEST_CASE(nextval_cycle_progressive) {
            int64_t val;

            sequence_init(hsequence, sequence, 1, 5, 1, 1, true);

            for (int i = 1; i <= 5; i++) {
                int ret = sequence_nextval(hsequence, sequence, &val);
                TEST_CHECK(ret == 0);
                TEST_CHECK(val == i);
            }

            /* Sixth call should cycle back to 1 */
            int ret = sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(ret == 0);
            TEST_CHECK(val == 1);

            /* Seventh call → 2 */
            ret = sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(ret == 0);
            TEST_CHECK(val == 2);
        }

        TEST_CASE(nextval_cycle_regressive) {
            int64_t val;

            sequence_init(hsequence, sequence, 1, 5, 5, -1, true);

            for (int i = 5; i >= 1; i--) {
                int ret = sequence_nextval(hsequence, sequence, &val);
                TEST_CHECK(ret == 0);
                TEST_CHECK(val == i);
            }

            /* Sixth call should cycle back to 5 */
            int ret = sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(ret == 0);
            TEST_CHECK(val == 5);

            /* Seventh call → 4 */
            ret = sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(ret == 0);
            TEST_CHECK(val == 4);
        }

        TEST_CASE(nextval_called_after_currval) {
            int64_t val;

            sequence_init(hsequence, sequence, 1, 100, 10, 5, false);

            /* nextval sets is_called */
            sequence_nextval(hsequence, sequence, &val);

            /* currval returns current */
            sequence_currval(hsequence, sequence, &val);
            TEST_CHECK(val == 10);

            /* nextval advances */
            sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(val == 15);
        }

    TEST_SUITE_END()

    TEST_SUITE(sequence_setval)

        TEST_CASE(setval_valid) {
            int64_t val;

            sequence_init(hsequence, sequence, 1, 100, 50, 1, false);

            TEST_CHECK(sequence_setval(hsequence, sequence, 75, true) == 0);

            sequence_currval(hsequence, sequence, &val);
            TEST_CHECK(val == 75);
        }

        TEST_CASE(setval_less_than_min) {
            sequence_init(hsequence, sequence, 1, 100, 50, 1, false);

            TEST_CHECK(sequence_setval(hsequence, sequence, 0, true) != 0);
        }

        TEST_CASE(setval_greater_than_max) {
            sequence_init(hsequence, sequence, 1, 100, 50, 1, false);

            TEST_CHECK(sequence_setval(hsequence, sequence, 101, true) != 0);
        }

        TEST_CASE(setval_with_is_called_false) {
            int64_t val;

            sequence_init(hsequence, sequence, 1, 100, 50, 1, false);

            TEST_CHECK(sequence_setval(hsequence, sequence, 75, false) == 0);

            /* currval should fail (is_called = false) */
            TEST_CHECK(sequence_currval(hsequence, sequence, &val) != 0);

            /* nextval returns the set value (not 76) */
            TEST_CHECK(sequence_nextval(hsequence, sequence, &val) == 0);
            TEST_CHECK(val == 75);
        }

    TEST_SUITE_END()

    TEST_SUITE(sequence_set_minval)

        TEST_CASE(set_minval_valid) {
            sequence_init(hsequence, sequence, 1, 100, 50, 1, false);

            TEST_CHECK(sequence_set_minval(hsequence, sequence, 25) == 0);
        }

        TEST_CASE(set_minval_greater_than_current) {
            sequence_init(hsequence, sequence, 1, 100, 50, 1, false);

            TEST_CHECK(sequence_set_minval(hsequence, sequence, 75) != 0);
        }

        TEST_CASE(set_minval_greater_equal_max) {
            sequence_init(hsequence, sequence, 1, 100, 50, 1, false);

            TEST_CHECK(sequence_set_minval(hsequence, sequence, 100) != 0);
            TEST_CHECK(sequence_set_minval(hsequence, sequence, 150) != 0);
        }

        TEST_CASE(set_minval_keeps_current) {
            int64_t val;

            sequence_init(hsequence, sequence, 1, 100, 50, 1, false);

            sequence_setval(hsequence, sequence, 75, true);
            sequence_set_minval(hsequence, sequence, 25);

            sequence_currval(hsequence, sequence, &val);
            TEST_CHECK(val == 75);
        }

    TEST_SUITE_END()

    TEST_SUITE(sequence_set_maxval)

        TEST_CASE(set_maxval_valid) {
            sequence_init(hsequence, sequence, 1, 100, 50, 1, false);

            TEST_CHECK(sequence_set_maxval(hsequence, sequence, 75) == 0);
        }

        TEST_CASE(set_maxval_less_than_current) {
            sequence_init(hsequence, sequence, 1, 100, 50, 1, false);

            TEST_CHECK(sequence_set_maxval(hsequence, sequence, 25) != 0);
        }

        TEST_CASE(set_maxval_less_equal_min) {
            sequence_init(hsequence, sequence, 1, 100, 50, 1, false);

            TEST_CHECK(sequence_set_maxval(hsequence, sequence, 1) != 0);
            TEST_CHECK(sequence_set_maxval(hsequence, sequence, 0) != 0);
        }

        TEST_CASE(set_maxval_keeps_current) {
            int64_t val;

            sequence_init(hsequence, sequence, 1, 100, 50, 1, false);

            sequence_setval(hsequence, sequence, 50, true);
            sequence_set_maxval(hsequence, sequence, 75);

            sequence_currval(hsequence, sequence, &val);
            TEST_CHECK(val == 50);
        }

    TEST_SUITE_END()

    TEST_SUITE(sequence_set_increment)

        TEST_CASE(set_increment_valid) {
            sequence_init(hsequence, sequence, 1, 100, 50, 1, false);

            TEST_CHECK(sequence_set_increment(hsequence, sequence, 5) == 0);
        }

        TEST_CASE(set_increment_zero) {
            sequence_init(hsequence, sequence, 1, 100, 50, 1, false);

            TEST_CHECK(sequence_set_increment(hsequence, sequence, 0) != 0);
        }

        TEST_CASE(set_increment_negative) {
            int64_t val;

            sequence_init(hsequence, sequence, 1, 100, 50, 1, false);

            TEST_CHECK(sequence_set_increment(hsequence, sequence, -3) == 0);

            sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(val == 50);

            sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(val == 47);
        }

        TEST_CASE(set_increment_affects_nextval) {
            int64_t val;

            sequence_init(hsequence, sequence, 1, 100, 1, 1, false);

            sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(val == 1);

            sequence_set_increment(hsequence, sequence, 5);

            sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(val == 6);

            sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(val == 11);
        }

    TEST_SUITE_END()

    TEST_SUITE(sequence_set_cycle)

        TEST_CASE(set_cycle_enable) {
            int64_t val;

            sequence_init(hsequence, sequence, 1, 5, 4, 1, false);

            sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(val == 4);

            sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(val == 5);

            /* Should fail without cycle */
            TEST_CHECK(sequence_nextval(hsequence, sequence, &val) != 0);

            /* Enable cycle */
            TEST_CHECK(sequence_set_cycle(hsequence, sequence, true) == 0);

            /* Should now cycle to min */
            TEST_CHECK(sequence_nextval(hsequence, sequence, &val) == 0);
            TEST_CHECK(val == 1);
        }

        TEST_CASE(set_cycle_disable) {
            int64_t val;

            sequence_init(hsequence, sequence, 1, 5, 4, 1, true);

            sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(val == 4);

            sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(val == 5);

            /* With cycle, should wrap to 1 */
            TEST_CHECK(sequence_nextval(hsequence, sequence, &val) == 0);
            TEST_CHECK(val == 1);

            /* Back to 5 */
            TEST_CHECK(sequence_setval(hsequence, sequence, 5, true) == 0);

            /* Disable cycle */
            TEST_CHECK(sequence_set_cycle(hsequence, sequence, false) == 0);

            /* Should now fail when exceeding max */
            TEST_CHECK(sequence_nextval(hsequence, sequence, &val) != 0);
        }

    TEST_SUITE_END()

    TEST_SUITE(sequence_complex)

        TEST_CASE(complex_scenario) {
            int64_t val;

            /* Create sequence for IDs: 1000 to 9999, step 1 */
            TEST_CHECK(sequence_init(hsequence, sequence, 1000, 9999, 1000, 1, false) == 0);

            /* Get 10 values */
            for (int i = 0; i < 10; i++) {
                TEST_CHECK(sequence_nextval(hsequence, sequence, &val) == 0);
                TEST_CHECK(val == 1000 + i);
            }

            /* Check currval */
            sequence_currval(hsequence, sequence, &val);
            TEST_CHECK(val == 1009);

            /* Change minval */
            TEST_CHECK(sequence_set_minval(hsequence, sequence, 500) == 0);

            /* Change maxval */
            TEST_CHECK(sequence_set_maxval(hsequence, sequence, 2000) == 0);

            /* Current should not change */
            sequence_currval(hsequence, sequence, &val);
            TEST_CHECK(val == 1009);

            /* Continue getting values */
            for (int i = 0; i < 5; i++) {
                TEST_CHECK(sequence_nextval(hsequence, sequence, &val) == 0);
                TEST_CHECK(val == 1010 + i);
            }
        }

        TEST_CASE(complex_cycle_scenario) {
            int64_t val;
            int expected[] = {1, 2, 3, 1, 2, 3, 1, 2, 3};
            int expected2[] = {1, 3, 1, 3, 1, 3};

            /* Create small cycling sequence */
            sequence_init(hsequence, sequence, 1, 3, 1, 1, true);

            /* Test several cycles */
            for (int i = 0; i < 9; i++) {
                TEST_CHECK(sequence_nextval(hsequence, sequence, &val) == 0);
                TEST_CHECK(val == expected[i]);
            }

            /* Change increment and cycle again */
            sequence_set_increment(hsequence, sequence, 2);
            sequence_setval(hsequence, sequence, 1, false);

            /* Now should go: 1, 3, 2, 1, 3, 2 */
            for (int i = 0; i < 6; i++) {
                TEST_CHECK(sequence_nextval(hsequence, sequence, &val) == 0);
                TEST_CHECK(val == expected2[i]);
            }
        }

        TEST_CASE(complex_edge_cases) {
            int64_t val;

            /* Sequence with single value (min == max - 1) */
            sequence_init(hsequence, sequence, 1, 2, 1, 1, false);

            sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(val == 1);

            sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(val == 2);

            TEST_CHECK(sequence_nextval(hsequence, sequence, &val) != 0);

            /* Sequence with large values */
            sequence_init(hsequence, sequence, -100, 100, 0, 10, false);

            sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(val == 0);

            sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(val == 10);

            sequence_setval(hsequence, sequence, -50, true);
            sequence_currval(hsequence, sequence, &val);
            TEST_CHECK(val == -50);

            sequence_set_increment(hsequence, sequence, -10);
            sequence_nextval(hsequence, sequence, &val);
            TEST_CHECK(val == -60);
        }

    TEST_SUITE_END()

TEST_END()

