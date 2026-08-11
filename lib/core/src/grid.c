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
#include <sys/param.h>

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
Datum grid_get_datum(Grid *hgrid, Grid *grid, uint16_t row, uint16_t column);
int grid_put_datum(Grid *hgrid, Grid *grid, uint16_t row, uint16_t column, Datum datum);
uint16_t grid_alloc_row(Grid *grid);

Column *hgrid_add_column(Grid *grid, const char *name, Type type, size_t size);
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

Datum
grid_get_datum(Grid *hgrid, Grid *grid, uint16_t row, uint16_t column) {
    const Column *hc = hgrid_get_column(hgrid, column);
    Cell c = grid_get_row(grid, row) + hc->offs;
    Datum ret;

    assert(column < hgrid->occupied);

    ret = (Datum){ .type = hc->type, .size = hc->size };

    switch (hc->type) {
        case T_UNKNOWN: assert(0 && "datum type T_UNKNOWN not supported"); break;
        case T_SMALLINT: ret.value.smallint = get_smallint(c); break;
        case T_INTEGER:  ret.value.integer = get_integer(c); break;
        case T_BIGINT:   ret.value.bigint = get_bigint(c); break;
        case T_CHAR:     ret.value.character = c; break;
        case T_VARCHAR:  ret.value.character = c; break;
        case T_MAX: assert(0 && "datum type T_MAX not supported"); break;
    }

    /* @todo: it is not the best idea to assign values to Datum in a switch
     * Find a better idea
     * Idea 1. Is it better to add a function pointer assingn_from_pointer() to SType sturct
     * and make an implementation for it for every type?
     */
    /* @todo: it makes incorrect size of varchar */
    return ret;
}

int
grid_put_datum(Grid *hgrid, Grid *grid, uint16_t row, uint16_t column, Datum datum) {
    const Column *hc = hgrid_get_column(hgrid, column);
    Cell c = grid_get_row(grid, row) + hc->offs;

    assert(column < hgrid->occupied);
    assert(datum.type != T_UNKNOWN);
    assert(datum.type != T_MAX);

    /* @todo it is better to allow to put datum with the same type group but not only
     * the same type
     */
    if (datum.type == g_types[hc->type].type) {
        switch (hc->type) {
            case T_UNKNOWN: assert(0 && "datum type T_UNKNOWN not supported"); break;
            case T_SMALLINT: put_smallint(c, datum.value.smallint); break;
            case T_INTEGER:  put_integer(c, datum.value.integer); break;
            case T_BIGINT:   put_bigint(c, datum.value.bigint); break;
            case T_CHAR:     put_char(c, datum.value.character, MIN(hc->size, datum.size)); break;
            case T_VARCHAR:  /* @todo make */ break;
            case T_MAX: assert(0 && "datum type T_MAX not supported"); break;
        }
    } else {
        assert(0 && "datum type mismatch");
        return 1;
    }

    /* @todo: it is not the best idea to assign values to Datum in a switch
     * Find a better idea
     * Idea 1. Is it better to add a function pointer assingn_from_pointer() to SType sturct
     * and make an implementation for it for every type?
     */
    /* @todo: it makes incorrect size of varchar */
    return 0;
}

uint16_t
grid_alloc_row(Grid *grid) {
    if (grid->occupied < grid->rown)
        return grid->occupied++;
    else
        return GRID_INVALID_IDX;
}

Column *
hgrid_add_column(Grid *grid, const char *name, Type type, size_t size) {
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
        hc->type = type;
        hc->size = size; /* hc->size means different for different size types
                          * e.g. for T_VARCHAR hc->size means maximum length of varchar
                          * and size in grid is defined in SType:size
                          * for T_CHAR the absolute size in grid is defined in hc->size
                          * for other types hc->size is ignored and must be 0
                          * and the absolute size in grid is defined in SType:size */

        /* Calculate byte offset based on the previous column */
        if (column_idx) {
            /* Correct size depending on type size meaning */
            Column *prev = grid_get_row(grid, column_idx - 1);

            switch (g_types[prev->type].size_meaning) {
                case SM_TYPESZ:
                    hc->offs = prev->offs + g_types[prev->type].size;
                    break;

                case SM_COLUMNSZ:
                    hc->offs = prev->offs + prev->size;
                    break;
            }
        } else
            hc->offs = 0;

        return hc;
    } else
        return NULL;
}

size_t
hgrid_get_row_size(Grid *grid) {
    if (grid->occupied) {
        const Column *hc = grid_get_row(grid, grid->occupied - 1);

        switch (g_types[hc->type].size_meaning) {
            case SM_TYPESZ:
                return hc->offs + g_types[hc->type].size;

            case SM_COLUMNSZ:
                return  hc->offs + hc->size;

            default:
                assert(0 && "Unknown size_meaning");
                return 0;
        }
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

