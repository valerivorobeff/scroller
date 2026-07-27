#include "mesh.h"

Mitor mesh_alloc_row(Grid *header, Grid *data);
Column *hmesh_add_column(Grid *grid, const char *name, Type type, size_t size);
void mitor_next(Mitor *mitor);

Mitor
mesh_alloc_row(Grid *header, Grid *data) {
    uint16_t row = grid_alloc_row(data);

    return (Mitor) { header, data, row };
}

Column *
hmesh_add_column(Grid *grid, const char *name, Type type, size_t size) {
    return hgrid_add_column(grid, name, type, size);
}

void
mitor_next(Mitor *mitor) {
    if (mitor_is_valid(*mitor))
        ++mitor->row;
}

