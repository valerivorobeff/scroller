#include "table.h"

Titor table_alloc_row(GidPair *tail);
Column *htable_add_column(Grid *grid, const char *name, Type type, size_t size);

Titor
table_alloc_row(GidPair *tail) {
    return mesh_alloc_row(tail);
}

Column *
htable_add_column(Grid *grid, const char *name, Type type, size_t size) {
    return hmesh_add_column(grid, name, type, size);
}


