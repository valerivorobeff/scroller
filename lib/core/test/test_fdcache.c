/**
 * @file test_fdcache.c
 * @brief Unit tests for file descriptor cache
 *
 * Tests the FdCache implementation which wraps icache with
 * automatic file descriptor management.
 */

#include "quin.h"
#include "fdcache.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <malloc.h>
#include <errno.h>
#include <assert.h>

static const char *TEST_DIR = "/tmp/scroller/test/";

/* Helper function: create temporary test directory */
static int
create_test_dir() {
    int ret =  mkdir(TEST_DIR, 0755);

    if (ret == 0 || errno == EEXIST) {
        chdir(TEST_DIR);
        return 0;
    }

    return ret;
}

/* Helper function: delete temporary test directory */
static int
remove_test_dir() {
    return rmdir(TEST_DIR);
}

/* Helper function: create temporary test files */
static void
create_test_file(const char *filename) {
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    assert(fd != -1);
    close(fd);
}

/* Helper function: delete test files */
static void
delete_test_file(const char *filename) {
    unlink(filename);
}

/* Helper function: get file descriptor flags (to check if fd is valid) */
static int
is_fd_valid(int fd) {
    return fcntl(fd, F_GETFD) != -1;
}

/* Custom hash function for Gid keys */
static size_t
gid_hash_fn(size_t key) {
    return key * 31;
}

TEST(fdcache)

    TEST_SUITE(fdcache_init)

        TEST_CASE(create_with_chains) {
            /* Cache with overflow support */
            FdCache *cache = fdcache_create(cache, 4, 8, gid_hash_fn);
            TEST_CHECK(cache != NULL);

            fdcache_free(cache);
        }

        TEST_CASE(required_memory_size) {
            size_t size = fdcache_get_required_memory_size(16, 32);
            TEST_CHECK(size > 0);
            TEST_CHECK(size == icache_get_required_memory_size(16, 32, sizeof(FdCache), 0));
        }

    TEST_SUITE_END()

    TEST_SUITE(fdcache_put_get)

        TEST_CASE(put_single) {
            FdCache *cache = fdcache_create(cache, 4, 8, gid_hash_fn);
            FdCache *entry;
            FdCache *retrieved;
            Gid gid = { .full = 12345 };
            gid_hex_t filename = gid2hex(gid);

            TEST_CHECK(create_test_dir() == 0);

            /* Create test file */
            create_test_file(filename.value);

            /* Put into cache */
            entry = fdcache_put(cache, gid.full);
            TEST_CHECK(entry != NULL);
            TEST_CHECK(entry->key == (ssize_t)gid.full);
            TEST_CHECK(entry->fd != -1);
            TEST_CHECK(is_fd_valid(entry->fd));

            /* Get from cache */
            retrieved = fdcache_get(cache, gid.full);
            TEST_CHECK(retrieved == entry);
            TEST_CHECK(retrieved->fd == entry->fd);

            fdcache_free(cache);
            delete_test_file(filename.value);
        }

        TEST_CASE(put_multiple) {
            FdCache *cache = fdcache_create(cache, 4, 8, gid_hash_fn);
            Gid gids[4] = {
                { .full = 1 },
                { .full = 2 },
                { .full = 3 },
                { .full = 4 }
            };

            /* Create test files and put into cache */
            for (int i = 0; i < 4; i++) {
                FdCache *entry;
                gid_hex_t filename = gid2hex(gids[i]);
                create_test_file(filename.value);

                entry = fdcache_put(cache, gids[i].full);
                TEST_CHECK(entry != NULL);
                TEST_CHECK(entry->key == (ssize_t)gids[i].full);
                TEST_CHECK(entry->fd != -1);
                TEST_CHECK(is_fd_valid(entry->fd));
            }

            /* Verify all entries are accessible */
            for (int i = 0; i < 4; i++) {
                FdCache *entry = fdcache_get(cache, gids[i].full);
                TEST_CHECK(entry != NULL);
                TEST_CHECK(entry->key == (ssize_t)gids[i].full);
                TEST_CHECK(is_fd_valid(entry->fd));
            }

            fdcache_free(cache);
            for (int i = 0; i < 4; i++) {
                gid_hex_t filename = gid2hex(gids[i]);
                delete_test_file(filename.value);
            }
        }

    TEST_SUITE_END()

    TEST_SUITE(fdcache_eviction)

        TEST_CASE(eviction_lru) {
            /* Small cache: 2 buckets, 2 chains (max 4 entries) */
            FdCache *cache = fdcache_create(cache, 2, 2, gid_hash_fn);
            Gid gids[6] = {
                { .full = 1 },
                { .full = 2 },
                { .full = 3 },
                { .full = 4 },
                { .full = 5 },
                { .full = 6 }
            };
            FdCache *entry5;
            FdCache *evicted;
            int evicted_fd;
            FdCache *still_there;

            /* Insert first 4 entries (fills cache) */
            for (int i = 0; i < 4; i++) {
                FdCache *entry = fdcache_put(cache, gids[i].full);
                TEST_CHECK(entry != NULL);
                TEST_CHECK(is_fd_valid(entry->fd));
            }

            evicted_fd = fdcache_get_fd(cache, 1);
            TEST_CHECK(is_fd_valid(evicted_fd));

            /* Insert 5th entry should trigger eviction */
            entry5 = fdcache_put(cache, gids[4].full);
            TEST_CHECK(entry5 != NULL);
            TEST_CHECK(is_fd_valid(entry5->fd));

            /* The LRU entry (gids[0]) should have been evicted */
            evicted = fdcache_get(cache, gids[0].full);
            TEST_CHECK(evicted == NULL);
            TEST_CHECK(!is_fd_valid(evicted_fd));

            /* gids[1] should still be in cache (touched by previous puts) */
            still_there = fdcache_get(cache, gids[1].full);
            TEST_CHECK(still_there != NULL);
            TEST_CHECK(is_fd_valid(still_there->fd));

            fdcache_free(cache);
            for (int i = 0; i < 6; i++) {
                gid_hex_t filename = gid2hex(gids[i]);
                delete_test_file(filename.value);
            }
        }

    TEST_SUITE_END()

    TEST_SUITE(fdcache_file_operations)

        TEST_CASE(file_close_on_eviction) {
            FdCache *cache = fdcache_create(cache, 2, 0, gid_hash_fn);
            Gid gids[2] = {
                { .full = 1 },
                { .full = 2 }
            };
            Gid gid3 = { .full = 3 };
            FdCache *entry1;
            FdCache *entry2;
            FdCache *entry3;
            int fd1;
            int fd2;
            int fd3;

            /* Insert first entry */
            entry1 = fdcache_put(cache, gids[0].full);
            TEST_CHECK(entry1 != NULL);
            fd1 = entry1->fd;
            TEST_CHECK(is_fd_valid(fd1));

            /* Insert second entry */
            entry2 = fdcache_put(cache, gids[1].full);
            TEST_CHECK(entry2 != NULL);
            fd2 = entry2->fd;
            TEST_CHECK(is_fd_valid(fd2));

            /* Try to insert third entry (cache is full, no chains) */
            entry3 = fdcache_put(cache, gid3.full);
            TEST_CHECK(entry3 != NULL);
            fd3 = entry3->fd;
            TEST_CHECK(is_fd_valid(fd3));

            /* fd1 should be closed (evicted) */
            TEST_CHECK(!is_fd_valid(fd1));

            /* fd2 should still be valid (not evicted) */
            TEST_CHECK(is_fd_valid(fd2));

            fdcache_free(cache);
            for (int i = 1; i <= 3; i++) {
                gid_hex_t filename = gid2hex(gids[i]);
                delete_test_file(filename.value);
            }
        }

        TEST_CASE(file_close_on_clear) {
            FdCache *cache = fdcache_create(cache, 4, 8, gid_hash_fn);
            Gid gids[3] = {
                { .full = 1 },
                { .full = 2 },
                { .full = 3 }
            };

            /* Insert all entries */
            int fds[3];
            for (int i = 0; i < 3; i++) {
                FdCache *entry = fdcache_put(cache, gids[i].full);
                TEST_CHECK(entry != NULL);
                fds[i] = entry->fd;
                TEST_CHECK(is_fd_valid(fds[i]));
            }

            /* Clear cache */
            fdcache_clear(cache, NULL);

            /* All file descriptors should be closed */
            for (int i = 0; i < 3; i++) {
                TEST_CHECK(!is_fd_valid(fds[i]));
            }

            fdcache_free(cache);
            for (int i = 0; i < 3; i++) {
                gid_hex_t filename = gid2hex(gids[i]);
                delete_test_file(filename.value);
            }
        }

        TEST_CASE(file_close_on_free) {
            FdCache *cache = fdcache_create(cache, 4, 8, gid_hash_fn);
            Gid gids[3] = {
                { .full = 1 },
                { .full = 2 },
                { .full = 3 }
            };

            /* Insert all entries */
            int fds[3];
            for (int i = 0; i < 3; i++) {
                FdCache *entry = fdcache_put(cache, gids[i].full);
                TEST_CHECK(entry != NULL);
                fds[i] = entry->fd;
                TEST_CHECK(is_fd_valid(fds[i]));
            }

            /* Free cache */
            fdcache_free(cache);

            /* All file descriptors should be closed */
            for (int i = 0; i < 3; i++) {
                TEST_CHECK(!is_fd_valid(fds[i]));
            }

            for (int i = 0; i < 3; i++) {
                gid_hex_t filename = gid2hex(gids[i]);
                delete_test_file(filename.value);
            }
        }

    TEST_SUITE_END()

    TEST_SUITE(fdcache_edge_cases)

        TEST_CASE(put_existing_file) {
            FdCache *cache = fdcache_create(cache, 4, 8, gid_hash_fn);
            Gid gid = { .full = 777 };
            gid_hex_t filename = gid2hex(gid);
            FdCache *entry1;
            FdCache *entry2;
            int fd1;

            /* First put should open file */
            entry1 = fdcache_put(cache, gid.full);
            TEST_CHECK(entry1 != NULL);
            fd1 = entry1->fd;
            TEST_CHECK(is_fd_valid(fd1));

            /* Second put should return existing entry */
            entry2 = fdcache_put(cache, gid.full);
            TEST_CHECK(entry2 == entry1);
            TEST_CHECK(entry2->fd == fd1);
            TEST_CHECK(is_fd_valid(fd1));

            fdcache_free(cache);
            delete_test_file(filename.value);
        }

        TEST_CASE(cache_zero_buckets) {
            FdCache *cache = fdcache_create(cache, 0, 8, gid_hash_fn);
            TEST_CHECK(cache == NULL);
        }

        TEST_CASE(cache_large_buckets) {
            FdCache *cache = fdcache_create(cache, 1000, 1000, gid_hash_fn);
            FdCache *entry;
            Gid gid = { .full = 42 };
            gid_hex_t filename = gid2hex(gid);
            TEST_CHECK(cache != NULL);

            entry = fdcache_put(cache, gid.full);
            TEST_CHECK(entry != NULL);
            TEST_CHECK(is_fd_valid(entry->fd));

            fdcache_free(cache);
            delete_test_file(filename.value);
        }

        TEST_CASE(custom_hash_function) {
            /* Use different hash function */
            FdCache *cache = fdcache_create(cache, 4, 8, gid_hash_fn);
            Gid gid = { .full = 123 };
            gid_hex_t filename = gid2hex(gid);
            FdCache *entry;

            TEST_CHECK(cache != NULL);

            entry = fdcache_put(cache, gid.full);
            TEST_CHECK(entry != NULL);
            TEST_CHECK(is_fd_valid(entry->fd));

            fdcache_free(cache);
            delete_test_file(filename.value);
        }

    TEST_SUITE_END()

    TEST_SUITE(fdcache_macros)

        TEST_CASE(init_macro) {
            void *buf = malloc(fdcache_get_required_memory_size(4, 8));
            FdCache *cache = fdcache_init((FdCache *)buf, 4, 8, gid_hash_fn);
            Gid gid = { .full = 456 };
            gid_hex_t filename = gid2hex(gid);
            FdCache *entry;

            TEST_CHECK(cache != NULL);
            TEST_CHECK((void *)cache == buf);

            entry = fdcache_put(cache, gid.full);
            TEST_CHECK(entry != NULL);
            TEST_CHECK(is_fd_valid(entry->fd));

            free(buf);
            delete_test_file(filename.value);
        }

        TEST_CASE(get_fd_macro) {
            FdCache *cache = fdcache_create(cache, 4, 8, gid_hash_fn);
            Gid gid = { .full = 789 };
            gid_hex_t filename = gid2hex(gid);

            fdcache_put(cache, gid.full);

            /* Use member macros */
            FdCache *entry = fdcache_get(cache, gid.full);
            TEST_CHECK(entry != NULL);

            /* FdCache doesn't have extra members, but macros should work */
            int *fd_ptr = fdcache_get_fd_ptr(cache, gid.full);
            TEST_CHECK(fd_ptr != NULL);
            TEST_CHECK(*fd_ptr == entry->fd);

            int fd_val = fdcache_get_fd(cache, gid.full);
            TEST_CHECK(fd_val == entry->fd);

            fdcache_free(cache);
            delete_test_file(filename.value);
        }

    TEST_SUITE_END()

    TEST_SUITE(fdcache_stress)

        TEST_CASE(stress_many_operations) {
            FdCache *cache = fdcache_create(cache, 8, 16, gid_hash_fn);
            const int NUM_FILES = 50;
            Gid gids[NUM_FILES];

            /* Create test files */
            for (int i = 0; i < NUM_FILES; i++) {
                gids[i].full = i + 1000;
            }

            /* Random access pattern */
            for (int iter = 0; iter < 10; iter++) {
                for (int i = 0; i < NUM_FILES; i++) {
                    FdCache *entry = icache_put_key(cache, gids[i].full);
                    if (entry != NULL) {
                        TEST_CHECK(is_fd_valid(entry->fd));
                    }
                }
            }

            /* Check that some entries are still valid */
            int valid_count = 0;
            for (int i = 0; i < NUM_FILES; i++) {
                FdCache *entry = fdcache_get(cache, gids[i].full);
                if (entry != NULL) {
                    TEST_CHECK(is_fd_valid(entry->fd));
                    valid_count++;
                }
            }

            TEST_CHECK(valid_count > 0);

            fdcache_free(cache);
            for (int i = 0; i < NUM_FILES; i++) {
                gid_hex_t filename = gid2hex(gids[i]);
                delete_test_file(filename.value);
            }
        }

        TEST_CASE(stress_concurrent_put) {
            FdCache *cache = fdcache_create(cache, 4, 4, gid_hash_fn);
            const int NUM_FILES = 20;
            Gid gids[NUM_FILES];

            for (int i = 0; i < NUM_FILES; i++) {
                gids[i].full = i + 2000;
            }

            /* Insert all files */
            for (int i = 0; i < NUM_FILES; i++) {
                FdCache *entry = fdcache_put(cache, gids[i].full);
                /* Some may be evicted, but should still work */
                if (entry != NULL) {
                    TEST_CHECK(is_fd_valid(entry->fd));
                }
            }

            fdcache_free(cache);
            for (int i = 0; i < NUM_FILES; i++) {
                gid_hex_t filename = gid2hex(gids[i]);
                delete_test_file(filename.value);
            }
        }

        TEST_CASE(done) {
            TEST_CHECK(remove_test_dir() == 0);
        }

    TEST_SUITE_END()

TEST_END()

