/**
 * @file test_type.c
 * @brief Unit tests for SQL type system
 *
 * Tests type conversion, comparison, and type group operations.
 */

#include "quin.h"
#include "type.h"
#include <string.h>
#include <stdio.h>

/* Helper function to compare Datum values */
static int datum_equal(Datum d1, Datum d2) {
    if (d1.type != d2.type) return 0;
    if (d1.size != d2.size) return 0;

    switch (g_types[d1.type].group) {
        case TG_INTEGER:
            return d1.value.bigint == d2.value.bigint;
        case TG_CHARACTER:
            return memcmp(d1.value.character, d2.value.character, d1.size) == 0;
        default:
            return 0;
    }
}

TEST(type)

    TEST_SUITE(check_init)

        TEST_CASE(check_type_groups) {
            for (int i = 0; i != TG_MAX; ++i)
                TEST_CHECK(i == g_type_groups[i].group);
        }

        TEST_CASE(check_types) {
            for (int i = 0; i != T_MAX; ++i)
                TEST_CHECK(i == g_types[i].type);
        }

    TEST_SUITE_END()

    TEST_SUITE(type_macros)

        TEST_CASE(make_smallint) {
            Datum d = make_smallint(42);
            TEST_CHECK(d.type == T_SMALLINT);
            TEST_CHECK(d.size == sizeof(int16_t));
            TEST_CHECK(d.value.smallint == 42);
        }

        TEST_CASE(make_integer) {
            Datum d = make_integer(12345);
            TEST_CHECK(d.type == T_INTEGER);
            TEST_CHECK(d.size == sizeof(int32_t));
            TEST_CHECK(d.value.integer == 12345);
        }

        TEST_CASE(make_bigint) {
            Datum d = make_bigint(9876543210LL);
            TEST_CHECK(d.type == T_BIGINT);
            TEST_CHECK(d.size == sizeof(int64_t));
            TEST_CHECK(d.value.bigint == 9876543210LL);
        }

        TEST_CASE(make_char) {
            char *str = "Hello";
            Datum d = make_char(str);
            TEST_CHECK(d.type == T_CHAR);
            TEST_CHECK(d.size == strlen(str));
            TEST_CHECK(d.value.character == str);
        }

        TEST_CASE(make_varchar) {
            char *str = "World";
            Datum d = make_varchar(str);
            TEST_CHECK(d.type == T_VARCHAR);
            TEST_CHECK(d.size == strlen(str));
            TEST_CHECK(d.value.character == str);
        }

    TEST_SUITE_END()

    TEST_SUITE(type_conversion)

        TEST_CASE(smallint2bigint) {
            Datum src = make_smallint(100);
            Datum dst = smallint2bigint(src);

            TEST_CHECK(dst.type == T_BIGINT);
            TEST_CHECK(dst.size == sizeof(int64_t));
            TEST_CHECK(dst.value.bigint == 100);
        }

        TEST_CASE(smallint2bigint_negative) {
            Datum src = make_smallint(-32768);
            Datum dst = smallint2bigint(src);

            TEST_CHECK(dst.type == T_BIGINT);
            TEST_CHECK(dst.value.bigint == -32768);
        }

        TEST_CASE(integer2bigint) {
            Datum src = make_integer(100000);
            Datum dst = integer2bigint(src);

            TEST_CHECK(dst.type == T_BIGINT);
            TEST_CHECK(dst.size == sizeof(int64_t));
            TEST_CHECK(dst.value.bigint == 100000);
        }

        TEST_CASE(integer2bigint_negative) {
            Datum src = make_integer(-2147483647);
            Datum dst = integer2bigint(src);

            TEST_CHECK(dst.type == T_BIGINT);
            TEST_CHECK(dst.value.bigint == -2147483647);
        }

        TEST_CASE(char2varchar_trim_spaces) {
            char *str = "Hello   ";
            Datum src = make_char(str);
            Datum dst = char2varchar(src);

            TEST_CHECK(dst.type == T_VARCHAR);
            TEST_CHECK(dst.size == 5);  /* "Hello" without spaces */
            TEST_CHECK(memcmp(dst.value.character, "Hello", 5) == 0);
        }

        TEST_CASE(char2varchar_no_spaces) {
            char *str = "Hello";
            Datum src = make_char(str);
            Datum dst = char2varchar(src);

            TEST_CHECK(dst.type == T_VARCHAR);
            TEST_CHECK(dst.size == 5);
            TEST_CHECK(memcmp(dst.value.character, "Hello", 5) == 0);
        }

        TEST_CASE(char2varchar_empty) {
            char *str = "";
            Datum src = make_char(str);
            Datum dst = char2varchar(src);

            TEST_CHECK(dst.type == T_VARCHAR);
            TEST_CHECK(dst.size == 0);
        }

        TEST_CASE(to_base_type_smallint) {
            Datum src = make_smallint(42);
            Datum dst = to_base_type(src);

            TEST_CHECK(dst.type == T_BIGINT);
            TEST_CHECK(dst.value.bigint == 42);
        }

        TEST_CASE(to_base_type_integer) {
            Datum src = make_integer(1000);
            Datum dst = to_base_type(src);

            TEST_CHECK(dst.type == T_BIGINT);
            TEST_CHECK(dst.value.bigint == 1000);
        }

        TEST_CASE(to_base_type_bigint) {
            Datum src = make_bigint(1234567890123LL);
            Datum dst = to_base_type(src);

            TEST_CHECK(dst.type == T_BIGINT);
            TEST_CHECK(dst.value.bigint == 1234567890123LL);
        }

        TEST_CASE(to_base_type_char) {
            char *str = "Test  ";
            Datum src = make_char(str);
            Datum dst = to_base_type(src);

            TEST_CHECK(dst.type == T_VARCHAR);
            TEST_CHECK(dst.size == 4);  /* "Test" without spaces */
        }

        TEST_CASE(to_base_type_varchar) {
            char *str = "Varchar";
            Datum src = make_varchar(str);
            Datum dst = to_base_type(src);

            TEST_CHECK(dst.type == T_VARCHAR);
            TEST_CHECK(dst.size == 7);
            TEST_CHECK(dst.value.character == str);
        }

    TEST_SUITE_END()

    TEST_SUITE(type_comparison_integer)

        TEST_CASE(cmp_integer_equal) {
            Datum d1 = make_smallint(42);
            Datum d2 = make_bigint(42);

            TEST_CHECK(cmp_integer(d1, d2) == 0);
            TEST_CHECK(eq_integer(d1, d2));
        }

        TEST_CASE(cmp_integer_less) {
            Datum d1 = make_smallint(10);
            Datum d2 = make_bigint(20);

            TEST_CHECK(cmp_integer(d1, d2) < 0);
            TEST_CHECK(lt_integer(d1, d2));
            TEST_CHECK(le_integer(d1, d2));
            TEST_CHECK(!gt_integer(d1, d2));
        }

        TEST_CASE(cmp_integer_greater) {
            Datum d1 = make_smallint(30);
            Datum d2 = make_bigint(20);

            TEST_CHECK(cmp_integer(d1, d2) > 0);
            TEST_CHECK(gt_integer(d1, d2));
            TEST_CHECK(ge_integer(d1, d2));
            TEST_CHECK(!lt_integer(d1, d2));
        }

        TEST_CASE(cmp_integer_negative) {
            Datum d1 = make_smallint(-5);
            Datum d2 = make_bigint(-3);

            TEST_CHECK(cmp_integer(d1, d2) < 0);
            TEST_CHECK(lt_integer(d1, d2));
        }

        TEST_CASE(cmp_integer_boundary) {
            Datum d1 = make_smallint(32767);
            Datum d2 = make_integer(32767);

            TEST_CHECK(cmp_integer(d1, d2) == 0);
        }

        TEST_CASE(cmp_integer_mixed_types) {
            Datum d1 = make_smallint(100);
            Datum d2 = make_integer(100);
            Datum d3 = make_bigint(100);

            TEST_CHECK(eq_integer(d1, d2));
            TEST_CHECK(eq_integer(d2, d3));
            TEST_CHECK(eq_integer(d1, d3));
        }

    TEST_SUITE_END()

    TEST_SUITE(type_comparison_character)

        TEST_CASE(cmp_character_equal) {
            char *s1 = "Hello";
            char *s2 = "Hello";
            Datum d1 = make_varchar(s1);
            Datum d2 = make_varchar(s2);

            TEST_CHECK(cmp_character(d1, d2) == 0);
            TEST_CHECK(eq_character(d1, d2));
        }

        TEST_CASE(cmp_character_char_varchar) {
            char *s1 = "Hello";
            char *s2 = "Hello";
            Datum d1 = make_char(s1);
            Datum d2 = make_varchar(s2);

            /* char → varchar (trim spaces) then compare */
            TEST_CHECK(cmp_character(d1, d2) == 0);
            TEST_CHECK(eq_character(d1, d2));
        }

        TEST_CASE(cmp_character_less) {
            char *s1 = "Apple";
            char *s2 = "Banana";
            Datum d1 = make_varchar(s1);
            Datum d2 = make_varchar(s2);

            TEST_CHECK(cmp_character(d1, d2) < 0);
            TEST_CHECK(lt_character(d1, d2));
            TEST_CHECK(le_character(d1, d2));
        }

        TEST_CASE(cmp_character_greater) {
            char *s1 = "Zebra";
            char *s2 = "Apple";
            Datum d1 = make_varchar(s1);
            Datum d2 = make_varchar(s2);

            TEST_CHECK(cmp_character(d1, d2) > 0);
            TEST_CHECK(gt_character(d1, d2));
            TEST_CHECK(ge_character(d1, d2));
        }

        TEST_CASE(cmp_character_prefix) {
            char *s1 = "Hello";
            char *s2 = "HelloWorld";
            Datum d1 = make_varchar(s1);
            Datum d2 = make_varchar(s2);

            /* "Hello" is prefix of "HelloWorld", so shorter is less */
            TEST_CHECK(cmp_character(d1, d2) < 0);
            TEST_CHECK(lt_character(d1, d2));
        }

        TEST_CASE(cmp_character_empty) {
            char *s1 = "";
            char *s2 = "Hello";
            Datum d1 = make_varchar(s1);
            Datum d2 = make_varchar(s2);

            TEST_CHECK(cmp_character(d1, d2) < 0);
            TEST_CHECK(lt_character(d1, d2));
        }

        TEST_CASE(cmp_character_spaces) {
            char *s1 = "Hello   ";
            char *s2 = "Hello";
            Datum d1 = make_char(s1);
            Datum d2 = make_varchar(s2);

            /* char trims spaces before comparison */
            TEST_CHECK(cmp_character(d1, d2) == 0);
            TEST_CHECK(eq_character(d1, d2));
        }

    TEST_SUITE_END()

    TEST_SUITE(type_type_groups)

        TEST_CASE(type_group_integer) {
            TEST_CHECK(g_types[T_SMALLINT].group == TG_INTEGER);
            TEST_CHECK(g_types[T_INTEGER].group == TG_INTEGER);
            TEST_CHECK(g_types[T_BIGINT].group == TG_INTEGER);
        }

        TEST_CASE(type_group_character) {
            TEST_CHECK(g_types[T_CHAR].group == TG_CHARACTER);
            TEST_CHECK(g_types[T_VARCHAR].group == TG_CHARACTER);
        }

        TEST_CASE(type_group_unknown) {
            TEST_CHECK(g_types[T_UNKNOWN].group == TG_UNKNOWN);
        }

        TEST_CASE(base_type_smallint) {
            TEST_CHECK(g_type_groups[TG_INTEGER].base_type == T_BIGINT);
        }

        TEST_CASE(base_type_character) {
            TEST_CHECK(g_type_groups[TG_CHARACTER].base_type == T_VARCHAR);
        }

    TEST_SUITE_END()

    TEST_SUITE(type_edge_cases)

        TEST_CASE(large_values) {
            Datum d1 = make_bigint(9223372036854775807LL);  /* INT64_MAX */
            Datum d2 = make_bigint(9223372036854775807LL);

            TEST_CHECK(eq_integer(d1, d2));
        }

        TEST_CASE(negative_values) {
            Datum d1 = make_bigint(-9223372036854775807LL);
            Datum d2 = make_bigint(-9223372036854775807LL);

            TEST_CHECK(eq_integer(d1, d2));
        }

        TEST_CASE(zero_values) {
            Datum d1 = make_smallint(0);
            Datum d2 = make_integer(0);
            Datum d3 = make_bigint(0);

            TEST_CHECK(eq_integer(d1, d2));
            TEST_CHECK(eq_integer(d2, d3));
        }

        TEST_CASE(character_case_sensitive) {
            char *s1 = "Hello";
            char *s2 = "hello";
            Datum d1 = make_varchar(s1);
            Datum d2 = make_varchar(s2);

            /* Should be case sensitive */
            TEST_CHECK(cmp_character(d1, d2) != 0);
            TEST_CHECK(!eq_character(d1, d2));
        }

        TEST_CASE(character_long_string) {
            char *s1 = "This is a very long string for testing";
            char *s2 = "This is a very long string for testing";
            Datum d1 = make_varchar(s1);
            Datum d2 = make_varchar(s2);

            TEST_CHECK(eq_character(d1, d2));
        }

        TEST_CASE(character_different_lengths) {
            char *s1 = "Short";
            char *s2 = "ShortLong";
            Datum d1 = make_varchar(s1);
            Datum d2 = make_varchar(s2);

            TEST_CHECK(lt_character(d1, d2));
        }

    TEST_SUITE_END()

    TEST_SUITE(type_conversion_chain)

        TEST_CASE(smallint_to_bigint_chain) {
            Datum src = make_smallint(42);
            Datum d1 = smallint2bigint(src);
            Datum d2 = to_base_type(src);

            TEST_CHECK(datum_equal(d1, d2));
        }

        TEST_CASE(integer_to_bigint_chain) {
            Datum src = make_integer(999);
            Datum d1 = integer2bigint(src);
            Datum d2 = to_base_type(src);

            TEST_CHECK(datum_equal(d1, d2));
        }

        TEST_CASE(char_to_varchar_chain) {
            char *str = "Hello   ";
            Datum src = make_char(str);
            Datum d1 = char2varchar(src);
            Datum d2 = to_base_type(src);

            TEST_CHECK(datum_equal(d1, d2));
        }

        TEST_CASE(no_conversion_chain) {
            char *str = "Hello";
            Datum src = make_varchar(str);
            Datum dst = to_base_type(src);

            /* Should be the same */
            TEST_CHECK(dst.type == src.type);
            TEST_CHECK(dst.size == src.size);
            TEST_CHECK(dst.value.character == src.value.character);
        }

    TEST_SUITE_END()

    TEST_SUITE(type_error_handling)

        TEST_CASE(invalid_type_assert) {
            /* This test is informational - asserts would trigger */
            /* In real code, bad casts would cause assert failures */
            Datum invalid = { .type = T_UNKNOWN, .size = 0, .value.unknown = NULL };
            Datum result = to_base_type(invalid);
            /* Should not reach here in normal execution */
            TEST_CHECK(result.type == T_UNKNOWN);
        }

    TEST_SUITE_END()

TEST_END()

