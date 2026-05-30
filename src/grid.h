#ifndef _GRID_H_
#define _GRID_H_

#include <stdint.h>
#include <stddef.h>

typedef void *Page;
typedef void *Row;
typedef void *Column;

typedef enum GridType : uint16_t {
    GT_FIXED = 1
} GridType;

typedef struct Grid {
    char        magic[4];
    uint16_t    size;
    GridType    type;
    uint16_t    rowsz;
    uint16_t    rown;
    uint16_t    occupied;
    char        reserved[2];
    char        datum[];
} Grid;

#define NAMESZ  16

typedef struct HColumn {
    char name[NAMESZ];
    size_t offs;
    size_t size;
} HColumn;

Grid *grid_init(Page page, uint16_t pagesz, GridType type, uint16_t rowsz);
Row grid_get_row(Grid *grid, uint16_t n);
Row grid_alloc_row(Grid *grid);
HColumn *hgrid_add_column(Grid *grid, const char *name, size_t size);
size_t hgrid_get_row_size(Grid *grid);

#endif /* _GRID_H_ */

