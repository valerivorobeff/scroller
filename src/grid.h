#ifndef _GRID_H_
#define _GRID_H_

#include <stdint.h>

typedef void *Page;
typedef void *Row;

typedef enum GridType : uint16_t {
    GT_FIXED = 1
} GridType;

typedef struct Grid {
    char        magic[4];
    uint16_t    size;
    GridType    type;
    uint16_t    rowsz;
    uint16_t    rown;
    char        reserved[4];
    char        datum[];
} Grid;

Grid *grid_init(Page page, uint16_t pagesz, GridType type, uint16_t rowsz);
Row grid_get_row(Grid *grid, uint16_t i);

#endif /* _GRID_H_ */

