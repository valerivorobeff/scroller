#include "table.h"

Titor table_alloc_row(Grid *header, Grid *data);
Column *htable_add_column(Grid *grid, const char *name, Type type, size_t size);

Titor
table_alloc_row(Grid *header, Grid *data) {
    /* @todo */
    return mesh_alloc_row((GidPair) { { .full = 0 }, { .full = 0 } });
}

Column *
htable_add_column(Grid *grid, const char *name, Type type, size_t size) {
    return hmesh_add_column(grid, name, type, size);
}


