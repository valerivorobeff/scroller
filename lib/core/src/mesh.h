/**
 * @file mesh.h
 * @brief Grid-based data storage system with fixed-size rows and columnar access
 *
 * @note This code uses GNU extensions (void* arithmetic) and requires
 *  GCC, Clang, or compatible compiler.
 *
 * This module provides a flexible grid storage system that supports both
 * header grids (for column definitions) and data grids (for actual data).
 * It is designed for efficient row and column operations on fixed-size
 * memory pages.
 */

#ifndef _MESH_H_
#define _MESH_H_

#include "grid.h"

typedef struct Mitor {
    Grid *header;
    Grid *data;
    uint16_t row;
} Mitor;

/**
 * @brief Initializes a new grid within a memory page.
 *
 * @param page      Pointer to the memory page where the grid will reside
 * @param pagesz    Size of the memory page in bytes
 * @param type      Type of grid to initialize (e.g., GT_FIXED)
 * @param rowsz     Size of each individual row in bytes
 * @return          Pointer to the initialized Grid structure, or NULL on error
 *
 * @note The page must provide at least pagesz bytes of contiguous memory.
 * @note The grid will be placed at the beginning of the page.
 */
#define mesh_init(page, pagesz, type, rowsz) \
    grid_init(page, pagesz, type, rowsz)

/**
 * @brief Allocates a new row in the grid.
 *
 * Finds the first unused row slot and marks it as occupied.
 *
 * @param grid      Pointer to the grid structure
 * @return          Index of the newly allocated row, or GRID_INVALID_IDXif grid is full
 *
 * @note The returned row's memory is zero-initialized.
 * @see grid_get_row()
 */
Mitor mesh_alloc_row(Grid *header, Grid *data);

/**
 * @brief Adds a new column definition to a header grid.
 *
 * @param grid      Pointer to the header grid (must contain Column entries)
 * @param name      Column name (must be unique within the grid)
 * @param type      Data type
 * @param size      Size of the column's data in bytes
 * @return          Pointer to the newly created Column structure, or NULL on error
 *
 * @note This function automatically calculates the byte offset for the new column
 *       based on previously added columns.
 * @note The header grid must have been initialized with row size sizeof(Column).
 */
Column *hmesh_add_column(Grid *grid, const char *name, Type type, size_t size);

#define hmesh_get_column_idx(grid, name) hgrid_get_column_idx(grid, name)

#define hmesh_init(page, pagesz, type) hgrid_init(page, pagesz, type)

#define dmesh_init(page, pagesz, type, hgrid) dgrid_init(page, pagesz, type, hgrid)

#define mesh_get_cell(itor, column) \
    grid_get_cell(itor.header, itor.data, itor.row, column)

#endif /* _MESH_H_ */

