#ifndef _CELL_H_
#define _CELL_H_

#include <stdint.h>
#include <string.h>

/**
 * @brief Opaque handle to a grid cell.
 *
 * Represents a single cell within a grid at a specific row and column.
*/
typedef void *Cell;

/**
 * Helper macros for reading/writing typed values from/to cells.
 */
#define put_smallint(c, val)    *(int16_t *)(c) = val
#define put_integer(c, val)     *(int32_t *)(c) = val
#define put_bigint(c, val)      *(int64_t *)(c) = val

#define get_smallint(c)         *(const int16_t *)(c)
#define get_integer(c)          *(const int32_t *)(c)
#define get_bigint(c)           *(const int64_t *)(c)

#define put_char(c, val, size)  strncpy(c, val, size - 1)

#endif /* _CELL_H_ */

