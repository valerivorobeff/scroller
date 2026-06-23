#include "sequence.h"
#include "grid.h"
#include <string.h>
#include <limits.h>

size_t
hgrid_get_column_size(Grid *grid, size_t row) {
    return ((HColumn *)(grid_get_row(grid, row)))->size;
}

void
hsequence_init() {
    char page[8096];
    Grid *hsequence = hgrid_init(page, 8096, GT_FIXED);

    hgrid_add_column(hsequence, "start", sizeof(int64_t));
    hgrid_add_column(hsequence, "finish", sizeof(int64_t));
    hgrid_add_column(hsequence, "current", sizeof(int64_t));
    hgrid_add_column(hsequence, "step", sizeof(int64_t));
}

void
sequence_init(Grid *hsequence, Page p) {
    Grid *g = dgrid_init(p, 8096, GT_FIXED, hsequence);

    Column c = dgrid_get_column(hsequence, g, 0, 0);
    memset(c, 0, hgrid_get_column_size(hsequence, 0));

    c = dgrid_get_column(hsequence, g, 0, 1);
    *(int64_t *)c = INT64_MAX;

    c = dgrid_get_column(hsequence, g, 0, 2);
    memset(c, 0, hgrid_get_column_size(hsequence, 0));

    c = dgrid_get_column(hsequence, g, 0, 3);
    *(int64_t *)c = 1;
}

