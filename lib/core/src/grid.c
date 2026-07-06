/**
 * @file grid.c
 * @brief Implementation of grid-based data storage system
 *
 * This file contains the implementation of functions for working with grid-based
 * data structures that support both fixed-size row storage and columnar storage
 * with dynamically defined schemas.
 *
 * The system supports two types of grids:
 * - Header grid (hgrid): stores column definitions (Column structures)
 * - Data grid (dgrid): stores actual row data with fixed-size rows
 *
 * Memory Layout:
 * ```
 * [Grid header][Row 0][Row 1][Row 2]...[Row N-1]
 * ```
 *
 * @note Memory pages must be pre-allocated by the caller
 * @warning All pages are zeroed during initialization for data security
 *
 * @see grid.h
 */

#include "grid.h"
#include <assert.h>
#include <string.h>

/**
 * Global variable PAGESZ with default value 4096
 * You should better recalculate it at the beginning of your application
 */
size_t PAGESZ = 4096;

/**
 * @cond PRIVATE
 * Forward declarations of public functions (implementation details)
 * @endcond
 */
Grid *grid_init(Page page, uint16_t pagesz, GridType type, uint16_t rowsz);
Row grid_get_row(Grid *grid, uint16_t n);
Cell grid_get_cell(Grid *hgrid, Grid *grid, uint16_t row, uint16_t column);
uint16_t grid_alloc_row(Grid *grid);

Column *hgrid_add_column(Grid *grid, const char *name, size_t size);
size_t hgrid_get_row_size(Grid *grid);
uint16_t hgrid_get_column_idx(Grid *grid, const char *name);

Grid *
grid_init(Page page, uint16_t pagesz, GridType type, uint16_t rowsz) {
    static const char magic[] = { 's', 'c', 'r', ' ' };
    Grid *g = (Grid *)page;

    assert(rowsz != 0);

    /* Compile-time/debug check for magic array size consistency */
    assert(sizeof(g->magic) == sizeof(magic));

    memset(page, 0, pagesz); /* We must zero the page due to security reasons */
    memcpy(g->magic, magic, sizeof(magic));
    g->size = pagesz;
    g->type = type;
    g->rowsz = rowsz;
    g->rown = (pagesz - sizeof(Grid)) / rowsz;

    return g;
}

Row
grid_get_row(Grid *grid, uint16_t n) {
    assert(n < grid->rown);
    return grid->datum + grid->rowsz * n;
}

Cell
grid_get_cell(Grid *hgrid, Grid *grid, uint16_t row, uint16_t column) {
    const Row r = grid_get_row(grid, row);
    const Column *hc = hgrid_get_column(hgrid, column);

    assert(column < hgrid->occupied);

    return r + hc->offs;
}

uint16_t
grid_alloc_row(Grid *grid) {
    if (grid->occupied < grid->rown)
        return grid->occupied++;
    else
        return GRID_INVALID_IDX;
}

Column *
hgrid_add_column(Grid *grid, const char *name, size_t size) {
    uint16_t column_idx = grid_alloc_row(grid);
    Column *hc;

    if (!grid_idx_valid(column_idx))
        return NULL;

    hc = grid_get_row(grid, column_idx);

    if (hc) {
        /* @todo: now the maximum copied bytes are NAMESZ - 1
         * which means that the latest byte should always be 0
         * and we can only work with 15 byte (NAMESZ - 1) length
         * strings, is it suitable? */
        strncpy(hc->name, name, NAMESZ - 1);
        hc->size = size;

        /* Calculate byte offset based on the previous column */
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
        const Column *hc = grid_get_row(grid, grid->occupied - 1);
        return hc->offs + hc->size;
    } else
       return 0;
}

uint16_t
hgrid_get_column_idx(Grid *grid, const char *name) {
    const uint16_t occupied = grid->occupied;
    Column *c = (Column *)grid->datum;

    for (size_t i = 0; i != occupied; ++i, ++c) {
        if (strcmp(c->name, name) == 0)
            return i;
    }

    return GRID_INVALID_IDX;
}

