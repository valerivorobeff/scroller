#include "sequence.h"
#include <string.h>
#include <limits.h>

size_t
hgrid_get_column_size(Grid *grid, size_t row) {
    return ((HColumn *)(grid_get_row(grid, row)))->size;
}

#define put_smallint(c, val)    *(int16_t *)(c) = val
#define put_integer(c, val)     *(int32_t *)(c) = val
#define put_bigint(c, val)      *(int64_t *)(c) = val

#define get_smallint(c)         *(int16_t *)(c)
#define get_integer(c)          *(int32_t *)(c)
#define get_bigint(c)           *(int64_t *)(c)

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
    put_bigint(c, 0);

    c = dgrid_get_column(hsequence, g, 0, 1);
    put_bigint(c, INT64_MAX);

    c = dgrid_get_column(hsequence, g, 0, 2);
    put_bigint(c, 0);

    c = dgrid_get_column(hsequence, g, 0, 3);
    put_bigint(c, 1);
}

int64_t
sequence_curval(Grid *hsequence, Grid *sequence) {
    Column c = dgrid_get_column(hsequence, sequence, 0, 2);
    return get_bigint(c);
}

int64_t
sequence_nextval(Grid *hsequence, Grid *sequence) {
    Column c = dgrid_get_column(hsequence, sequence, 0, 2);
    int64_t current = get_bigint(c);
    int64_t newval = current + get_bigint(dgrid_get_column(hsequence, sequence, 0, 3));
    put_bigint(c, newval);

    return newval;
}

