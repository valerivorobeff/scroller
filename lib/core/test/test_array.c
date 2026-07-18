#include "quin.h"
#include "array.h"
#include "memory.h"

TEST(array)

    TEST_SUITE(array_basic)

        TEST_CASE(test1) {
            memory_init_default();
            int *a = array_create(a, 16);

            array_put(a, 5);

            TEST_CHECK(a[0] == 5);
            TEST_CHECK(array_back(a) == 5);
            TEST_CHECK(array_size(a) == 1);


            memory_destroy();
        }

    TEST_SUITE_END()

TEST_END()

