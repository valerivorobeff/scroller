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
grid_get_row(Grid *grid, uint16_t i) {
    return grid->datum + grid->rowsz * i;
}

typedef struct HColumn {
    char name[16];
    size_t offs;
    size_t size;
} HColumn;

int
main() {
    HColumn hcolumn;
    Page *hp = malloc(8096), *p = malloc(8096);
    Grid *hg = grid_init(hp, 8096, GT_FIXED, sizeof(HColumn)), *g;
    Row row;

    /* CREATE TABLE HEADER */
    /* ALTER TABLE ADD COLUMN 0 */
    strcpy(hcolumn.name, "column 0");
    hcolumn.size = 4;

    row = grid_get_row(hg, 0);
    memcpy(row, &hcolumn, sizeof(HColumn));

    /* ALTER TABLE ADD COLUMN 1 */
    strcpy(hcolumn.name, "column 1");
    hcolumn.size = 12;

    row = grid_get_row(hg, 1);
    memcpy(row, &hcolumn, sizeof(HColumn));

    /* CREATE TABLE DATA */
    g = grid_init(p, 8096, GT_FIXED, 16);

    /* INSERT */
    row = grid_get_row(g, 0);
    strcpy(row + 0, "123");
    strcpy(row + 4, "456789");

    return 0;
}

