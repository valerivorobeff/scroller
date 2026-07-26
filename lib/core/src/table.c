#include "table.h"

Titor table_alloc_row(Grid *header, Grid *data);
Column *htable_add_column(Grid *grid, const char *name, Type type, size_t size);

Titor
table_alloc_row(Grid *header, Grid *data) {
    return mesh_alloc_row(header, data);
}

Column *
htable_add_column(Grid *grid, const char *name, Type type, size_t size) {
    return hmesh_add_column(grid, name, type, size);
}


