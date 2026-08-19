/**
 * @file align.h
 * @brief Align manupulations
 */

#ifndef _ALIGN_H_
#define _ALIGN_H_

#include <stddef.h>

/**
 * @brief Align size up to specified alignment
 * @param sz Size to align
 * @param align Alignment (must be power of two)
 * @return Aligned size
 */
size_t align_up(size_t sz, int align);

/**
 * @brief Align size to maximum alignment
 * @param sz Size to align
 * @return Aligned size
 */
size_t align_max(size_t sz);

#endif /* _ALIGN_H_ */

