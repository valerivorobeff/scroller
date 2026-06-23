/**
 * @file gid.h
 * @brief Global id
 *
 */

#ifndef _GID_H_
#define _GID_H_

#include <stdint.h>

/**
 * @brief Union to store gid.
 *
 */
typedef union Gid {
    uint64_t full;
    struct {
        uint64_t file_id    : 40;
        uint64_t page       : 24;
    } parts;
} Gid;

#endif /* _GID_H_ */

