#include "grid.h"
#include <assert.h>

#include <string.h>

Grid *
grid_init(Page page, uint16_t pagesz, GridType type, uint16_t rowsz) {
    static const char magic[] = { 's', 'c', 'r', ' ' };
    Grid *g = (Grid *)page;

    assert(sizeof(g->magic) == sizeof(magic));

    memset(page, 0, pagesz); /* We must zero the page due to sequrity reasons */
    g->size = pagesz;
    g->type = type;
    g->rowsz = rowsz;
    g->rown = (pagesz - sizeof(Grid)) / rowsz;

    return g;
}

Row
grid_get_row(Grid *grid, uint16_t n) {
    return grid->datum + grid->rowsz * n;
}

Row
grid_alloc_row(Grid *grid) {
    if (grid->occupied < grid->rown)
        return grid_get_row(grid, grid->occupied++);
    else
        return NULL;
}

HColumn *
hgrid_add_column(Grid *grid, const char *name, size_t size) {
    HColumn *hc = grid_alloc_row(grid);

    if (hc) {
        strncpy(hc->name, name, NAMESZ);
        hc->size = size;

        if (grid->occupied > 1)
            hc->offs = (hc - 1)->offs + size;
        else
            hc->offs = 0;

        return hc;
    } else
        return NULL;
}

size_t
hgrid_get_row_size(Grid *grid) {
    if (grid->occupied) {
        HColumn *hc = grid_get_row(grid, grid->occupied - 1);
        return hc->offs + hc->size;
    } else
       return 0;
}

