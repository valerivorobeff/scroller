/**
 * @file align.c
 * @brief Align manipulations
 */

#include "align.h"
#include <unistd.h>

/** @brief Maximum alignment for allocation */
#define MAX_ALIGN _Alignof(max_align_t)

size_t align_up(size_t sz, int align);
size_t align_max(size_t sz);

/**
 * @brief Align size up to specified alignment
 * @param sz Size to align
 * @param align Alignment (must be power of two)
 * @return Aligned size
 */
size_t
align_up(size_t sz, int align) {
    return (sz + align - 1) & ~(align - 1);
}

/**
 * @brief Align size to maximum alignment
 * @param sz Size to align
 * @return Aligned size
 */
size_t
align_max(size_t sz) {
    return align_up(sz, MAX_ALIGN);
}

