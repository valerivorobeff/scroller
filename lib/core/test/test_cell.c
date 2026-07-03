#include "quin.h"
#include "cell.h"

TEST(cell)

    /**
     * @brief Test suite for cell operations
     *
     */
    TEST_SUITE(cell)

        TEST_CASE(int) {
            char buf[16];

            put_smallint(buf, 500);
            TEST_CHECK(get_smallint(buf) == 500);

            put_integer(buf, 500500);
            TEST_CHECK(get_integer(buf) == 500500);

            put_bigint(buf, 5005004);
            TEST_CHECK(get_bigint(buf) == 5005004);
        }

        TEST_CASE(char) {
            char buf[16];
            const char *str16 = "0123456789abcde";

            put_char(buf, str16, sizeof(buf));
            TEST_CHECK(strcmp(buf, str16) == 0);
        }

    TEST_SUITE_END()  /* End of cell test suite */

TEST_END()  /* End of cell test unit */

