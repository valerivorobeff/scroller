#ifndef _TABLE_H_
#define _TABLE_H_

#include "mesh.h"

typedef Mitor Titor;

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
#define table_init(page, pagesz, type, rowsz) \
    mesh_init(page, pagesz, type, rowsz)

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
Mitor table_alloc_row(Grid *header, Grid *data);

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
Column *htable_add_column(Grid *grid, const char *name, Type type, size_t size);

#define htable_get_column_idx(grid, name) hmesh_get_column_idx(grid, name)

#define htable_init(page, pagesz, type) hmesh_init(page, pagesz, type)

#define dtable_init(page, pagesz, type, hgrid) dmesh_init(page, pagesz, type, hgrid)

#define table_get_cell(itor, column) mesh_get_cell(itor, column)

#endif /* _TABLE_H_ */

