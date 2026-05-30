#include "grid.h"
#include <assert.h>

#include <malloc.h>
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

int
main() {
    Page *hp = malloc(8096), *p = malloc(8096);
    Grid *hg = grid_init(hp, 8096, GT_FIXED, sizeof(HColumn)), *g;
    Row row;

    /* CREATE TABLE HEADER */
    /* ALTER TABLE ADD COLUMN */
    if (!hgrid_add_column(hg, "id_user", 4)) {

    }
    /* ALTER TABLE ADD COLUMN */
    if (!hgrid_add_column(hg, "name", 12)) {

    }

    /* CREATE TABLE DATA */
    g = grid_init(p, 8096, GT_FIXED, hgrid_get_row_size(hg));

    /* INSERT ROW INTO TABLE */
    row = grid_alloc_row(g);    /* alloc row */
    if (row) {
        HColumn *hc = grid_get_row(hg, 0);
        Column c = row + hc->offs;
        strncpy(c, "123", hc->size);

        hc = grid_get_row(hg, 1);
        c = row + hc->offs;
        strncpy(c, "12345678", hc->size);
    }

    return 0;
}

