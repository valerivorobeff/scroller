/**
 * @file gid.h
 * @brief Global id
 *
 */

#ifndef _GID_H_
#define _GID_H_

#include <stdint.h>

/**
 * @note total size must be 64 bits
 */
#define GID_FILEIDBITS  40
#define GID_PAGEBITS    24

#define GID_OCTETBITS   8

/**
 * @brief Union to store gid.
 *
 */
typedef union Gid {
    uint64_t full;
    struct {
        uint64_t file_id    : GID_FILEIDBITS; /* This field should be first in the struct */
        uint64_t page       : GID_PAGEBITS;
    } parts;
} Gid;

typedef struct gid_hex_t {
    char value[GID_FILEIDBITS + 1];
} gid_hex_t;

gid_hex_t gid2hex(Gid gid);

#endif /* _GID_H_ */

