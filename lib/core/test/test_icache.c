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
#include "grid.h"
//#include "ilist2.h"
//#include "ihash.h"
#include "icache.h"
#include <malloc.h>
#include <string.h>


/* ==============================
 *
 * Test suite icache
 *
 ================================ */

static size_t custom_hash_fn(size_t key) {
    return key * 31;
}

TEST(icache)

    typedef struct centry {
        ssize_t key;
        int value;
    } centry;

    TEST_SUITE(basic)

        TEST_CASE(create) {
            centry *cache = icache_create(cache, 4, 8, NULL);
            icache *ic = (icache *)cache;

            TEST_CHECK(cache != NULL);
            TEST_CHECK(ic->hashoffs == sizeof(icache));

            /* Verify hash and list are empty */
            TEST_CHECK(icache_get(cache, 1) == NULL);
            TEST_CHECK(icache_exists(cache, 1) == 0);

            icache_free(cache);
        }

        TEST_CASE(put_get) {
            centry *cache = icache_create(cache, 4, 8, NULL);

            /* Insert a value */
            centry *e = icache_put(cache, 42, 100);
            TEST_CHECK(e != NULL);
            TEST_CHECK(e->key == 42);
            TEST_CHECK(e->value == 100);

            /* Retrieve it */
            e = icache_get(cache, 42);
            TEST_CHECK(e != NULL);
            TEST_CHECK(e->key == 42);
            TEST_CHECK(e->value == 100);

            /* Update existing */
            e = icache_put(cache, 42, 200);
            TEST_CHECK(e != NULL);
            TEST_CHECK(e->key == 42);
            TEST_CHECK(e->value == 200);

            e = icache_get(cache, 42);
            TEST_CHECK(e != NULL);
            TEST_CHECK(e->value == 200);

            /* Non-existent key */
            TEST_CHECK(icache_get(cache, 999) == NULL);
            TEST_CHECK(icache_exists(cache, 999) == 0);

            icache_free(cache);
        }

        TEST_CASE(init) {
            size_t size = icache_get_required_memory_size(4, 8, sizeof(centry));
            centry *cache = malloc(size);

            TEST_CHECK(cache != NULL);

            cache = icache_init(cache, 4, 8, NULL);
            TEST_CHECK(cache != NULL);

            centry *e = icache_put(cache, 42, 100);
            TEST_CHECK(e != NULL);
            TEST_CHECK(e->value == 100);

            e = icache_get(cache, 42);
            TEST_CHECK(e != NULL);
            TEST_CHECK(e->value == 100);

            free(cache);
        }

        TEST_CASE(clear) {
            centry *cache = icache_create(cache, 4, 8, NULL);

            icache_put(cache, 1, 100);
            icache_put(cache, 2, 200);
            icache_put(cache, 3, 300);

            TEST_CHECK(icache_get(cache, 1) != NULL);
            TEST_CHECK(icache_get(cache, 2) != NULL);
            TEST_CHECK(icache_get(cache, 3) != NULL);

            icache_clear(cache, NULL);

            TEST_CHECK(icache_get(cache, 1) == NULL);
            TEST_CHECK(icache_get(cache, 2) == NULL);
            TEST_CHECK(icache_get(cache, 3) == NULL);

            /* Should work after clear */
            icache_put(cache, 10, 1000);
            TEST_CHECK(icache_get(cache, 10)->value == 1000);

            icache_free(cache);
        }
    TEST_SUITE_END()  /* End of basic test suite */

    TEST_SUITE(collision)

        TEST_CASE(collision) {
            centry *cache = icache_create(cache, 4, 8, NULL);

            /* Keys that hash to same bucket (identity hash) */
            icache_put(cache, 4, 400);   /* bucket 0 */
            icache_put(cache, 8, 800);   /* bucket 0 - collision */
            icache_put(cache, 12, 1200); /* bucket 0 - collision */

            TEST_CHECK(icache_get(cache, 4)->value == 400);
            TEST_CHECK(icache_get(cache, 8)->value == 800);
            TEST_CHECK(icache_get(cache, 12)->value == 1200);

            /* Update value in chain */
            icache_put(cache, 8, 888);
            TEST_CHECK(icache_get(cache, 8)->value == 888);

            icache_free(cache);
        }

    TEST_SUITE_END()  /* End of collision test suite */

    TEST_SUITE(lru)

        TEST_CASE(lru_eviction) {
            /* Cache with 4 slots total (2 buckets + 2 chain nodes) */
            centry *cache = icache_create(cache, 2, 2, NULL);

            /* Fill the cacee */
            icache_put(cache, 1, 100);
            icache_put(cache, 2, 200);
            icache_put(cache, 3, 300);
            icache_put(cache, 4, 400);

            /* All entries should exist */
            TEST_CHECK(icache_get(cache, 1) != NULL);
            TEST_CHECK(icache_get(cache, 2) != NULL);
            TEST_CHECK(icache_get(cache, 3) != NULL);
            TEST_CHECK(icache_get(cache, 4) != NULL);

            /* Insert new entry, should evict LRU (key 1) */
            icache_put(cache, 5, 500);

            TEST_CHECK(icache_get(cache, 1) == NULL);  /* Evicted */
            TEST_CHECK(icache_get(cache, 2) != NULL);
            TEST_CHECK(icache_get(cache, 3) != NULL);
            TEST_CHECK(icache_get(cache, 4) != NULL);
            TEST_CHECK(icache_get(cache, 5) != NULL);

            icache_free(cache);
        }

        TEST_CASE(lru_access_moves_to_front) {
            centry *cache = icache_create(cache, 2, 2, NULL);

            /* Fill cache */
            icache_put(cache, 1, 100);
            icache_put(cache, 2, 200);
            icache_put(cache, 3, 300);
            icache_put(cache, 4, 400);

            /* Access key 1 (should become most recently used) */
            TEST_CHECK(icache_put_key(cache, 1) != NULL);

            /* Insert new entry, should evict LRU (key 2) */
            icache_put(cache, 5, 500);

            TEST_CHECK(icache_get(cache, 1) != NULL);  /* Should survive */
            TEST_CHECK(icache_get(cache, 2) == NULL);  /* Evicted */
            TEST_CHECK(icache_get(cache, 3) != NULL);
            TEST_CHECK(icache_get(cache, 4) != NULL);
            TEST_CHECK(icache_get(cache, 5) != NULL);

            icache_free(cache);
        }

        TEST_CASE(lru_update_moves_to_front) {
            centry *cache = icache_create(cache, 2, 2, NULL);

            /* Fill cache */
            icache_put(cache, 1, 100);
            icache_put(cache, 2, 200);
            icache_put(cache, 3, 300);
            icache_put(cache, 4, 400);

            /* Update key 1 (should become most recently used) */
            icache_put(cache, 1, 150);

            /* Insert new entry, should evict LRU (key 2) */
            icache_put(cache, 5, 500);

            TEST_CHECK(icache_get(cache, 1) != NULL);  /* Should survive */
            TEST_CHECK(icache_get(cache, 2) == NULL);  /* Evicted */
            TEST_CHECK(icache_get(cache, 3) != NULL);
            TEST_CHECK(icache_get(cache, 4) != NULL);
            TEST_CHECK(icache_get(cache, 5) != NULL);

            icache_free(cache);
        }

        TEST_CASE(lru_eviction_chain) {
            /* Cache with small size */
            centry *cache = icache_create(cache, 1, 1, NULL);

            /* Fill cache (all go to same bucket, causing chain) */
            icache_put(cache, 1, 100);
            icache_put(cache, 2, 200);
            icache_put(cache, 3, 300);  /* Should evict LRU */

            TEST_CHECK(icache_get(cache, 1) == NULL);  /* Evicted */
            TEST_CHECK(icache_get(cache, 2) != NULL);
            TEST_CHECK(icache_get(cache, 3) != NULL);

            /* Access key 2 to make it MRU */
            TEST_CHECK(icache_put_key(cache, 2) != NULL);

            /* Insert new entry */
            icache_put(cache, 4, 400);  /* Should evict key 3 */

            TEST_CHECK(icache_get(cache, 2) != NULL);  /* Survives */
            TEST_CHECK(icache_get(cache, 3) == NULL);  /* Evicted */
            TEST_CHECK(icache_get(cache, 4) != NULL);

            icache_free(cache);
        }

        TEST_CASE(lru_full_capacity) {
            centry *cache = icache_create(cache, 2, 0, NULL);  /* No overflow nodes */

            /* Fill 2 buckets */
            icache_put(cache, 1, 100);
            icache_put(cache, 2, 200);

            /* Third should evict LRU (key 1) */
            icache_put(cache, 3, 300);

            TEST_CHECK(icache_get(cache, 1) == NULL);
            TEST_CHECK(icache_get(cache, 2) != NULL);
            TEST_CHECK(icache_get(cache, 3) != NULL);

            /* Fourth should evict LRU (key 2) */
            icache_put(cache, 4, 400);

            TEST_CHECK(icache_get(cache, 2) == NULL);
            TEST_CHECK(icache_get(cache, 3) != NULL);
            TEST_CHECK(icache_get(cache, 4) != NULL);

            icache_free(cache);
        }

    TEST_SUITE_END()  /* End of lru test suite */

    TEST_SUITE(helpers)

        TEST_CASE(put_key) {
            struct set_entry {
                ssize_t key;
                /* no value field */
            };

            struct set_entry *cache = icache_create(cache, 4, 8, NULL);
            TEST_CHECK(cache != NULL);

            icache_put_key(cache, 100);
            icache_put_key(cache, 200);
            icache_put_key(cache, 300);

            TEST_CHECK(icache_exists(cache, 100));
            TEST_CHECK(icache_exists(cache, 200));
            TEST_CHECK(icache_exists(cache, 300));
            TEST_CHECK(!icache_exists(cache, 400));

            icache_free(cache);
        }

        TEST_CASE(put_struct) {
            centry *cache = icache_create(cache, 4, 8, NULL);

            centry data1 = {42, 100};
            centry data2 = {43, 200};

            icache_put_struct(cache, &data1);
            icache_put_struct(cache, &data2);

            TEST_CHECK(icache_get(cache, 42)->value == 100);
            TEST_CHECK(icache_get(cache, 43)->value == 200);

            /* Update with struct */
            centry data1_new = {42, 999};
            icache_put_struct(cache, &data1_new);
            TEST_CHECK(icache_get(cache, 42)->value == 999);

            icache_free(cache);
        }

        TEST_CASE(get_member_ptr) {
            centry *cache = icache_create(cache, 4, 8, NULL);
            icache_put(cache, 42, 100);

            int *val = icache_get_member_ptr(cache, 42, value);
            TEST_CHECK(val != NULL);
            TEST_CHECK(*val == 100);

            *val = 999;
            TEST_CHECK(icache_get(cache, 42)->value == 999);

            TEST_CHECK(icache_get_member_ptr(cache, 999, value) == NULL);

            icache_free(cache);
        }

        TEST_CASE(get_member) {
            centry *cache = icache_create(cache, 4, 8, NULL);
            icache_put(cache, 42, 100);

            int val = icache_get_member(cache, 42, value);
            TEST_CHECK(val == 100);

            val = icache_get_member(cache, 999, value);
            TEST_CHECK(val == 0);

            icache_free(cache);
        }

        TEST_CASE(get_value_ptr) {
            centry *cache = icache_create(cache, 4, 8, NULL);
            icache_put(cache, 42, 100);

            int *val = icache_get_value_ptr(cache, 42);
            TEST_CHECK(val != NULL);
            TEST_CHECK(*val == 100);

            icache_free(cache);
        }

        TEST_CASE(get_value) {
            centry *cache = icache_create(cache, 4, 8, NULL);
            icache_put(cache, 42, 100);

            int val = icache_get_value(cache, 42);
            TEST_CHECK(val == 100);

            icache_free(cache);
        }

    TEST_SUITE_END()  /* End of helpers test suite */

    TEST_SUITE(edge_cases)

        TEST_CASE(zero_chains) {
            centry *cache = icache_create(cache, 4, 0, NULL);

            /* Can still store 4 entries */
            icache_put(cache, 1, 100);
            icache_put(cache, 2, 200);
            icache_put(cache, 3, 300);
            icache_put(cache, 4, 400);

            TEST_CHECK(icache_get(cache, 1) != NULL);
            TEST_CHECK(icache_get(cache, 2) != NULL);
            TEST_CHECK(icache_get(cache, 3) != NULL);
            TEST_CHECK(icache_get(cache, 4) != NULL);

            /* 5th should evict LRU */
            icache_put(cache, 5, 500);
            TEST_CHECK(icache_get(cache, 1) == NULL);
            TEST_CHECK(icache_get(cache, 5) != NULL);

            icache_free(cache);
        }

        TEST_CASE(single_bucket) {
            centry *cache = icache_create(cache, 1, 8, NULL);

            /* All keys go to same bucket */
            icache_put(cache, 1, 100);
            icache_put(cache, 2, 200);
            icache_put(cache, 3, 300);
            icache_put(cache, 4, 400);
            icache_put(cache, 5, 500);
            icache_put(cache, 6, 600);
            icache_put(cache, 7, 700);
            icache_put(cache, 8, 800);
            icache_put(cache, 9, 900);

            /* Total capacity: bucketsz(1) + chainsz(8) = 9 */
            TEST_CHECK(icache_get(cache, 1) != NULL);
            TEST_CHECK(icache_get(cache, 2) != NULL);
            TEST_CHECK(icache_get(cache, 3) != NULL);
            TEST_CHECK(icache_get(cache, 4) != NULL);
            TEST_CHECK(icache_get(cache, 5) != NULL);
            TEST_CHECK(icache_get(cache, 6) != NULL);
            TEST_CHECK(icache_get(cache, 7) != NULL);
            TEST_CHECK(icache_get(cache, 8) != NULL);
            TEST_CHECK(icache_get(cache, 9) != NULL);

            /* 5th should evict LRU (key 1) */
            icache_put(cache, 10, 1000);
            TEST_CHECK(icache_get(cache, 1) == NULL);
            TEST_CHECK(icache_get(cache, 10) != NULL);
            icache_free(cache);
        }

        TEST_CASE(zero_buckets) {
            centry *cache = icache_create(cache, 0, 8, NULL);
            TEST_CHECK(cache == NULL);  /* Should fail */
        }

        TEST_CASE(custom_hash) {
            centry *cache = icache_create(cache, 4, 8, custom_hash_fn);
            TEST_CHECK(cache != NULL);

            icache_put(cache, 1, 100);
            icache_put(cache, 2, 200);

            TEST_CHECK(icache_get(cache, 1)->value == 100);
            TEST_CHECK(icache_get(cache, 2)->value == 200);

            icache_free(cache);
        }

        TEST_CASE(large_values) {
            struct large_entry {
                ssize_t key;
                char data[256];
                int value;
            };

            struct large_entry *cache = icache_create(cache, 4, 8, NULL);
            TEST_CHECK(cache != NULL);

            struct large_entry e1 = {42, "Hello, World!", 100};
            icache_put_struct(cache, &e1);

            struct large_entry *e = icache_get(cache, 42);
            TEST_CHECK(e != NULL);
            TEST_CHECK(e->key == 42);
            TEST_CHECK(e->value == 100);
            TEST_CHECK(strcmp(e->data, "Hello, World!") == 0);

            icache_free(cache);
        }

    TEST_SUITE_END()  /* End of edge_cases test suite */

TEST_END()

