#ifndef _TABLE_H_
#define _TABLE_H_

#include "mesh.h"

typedef struct Tran Tran;

typedef struct Titor {
    Mitor m;
    Tran *tran;
} Titor;

Titor titor_init(Tran *tran, GidPair gp, Grid *header, Grid *data);
#define titor_init_invalid() (Titor){ .m = mitor_init_invalid(), .tran = NULL }
#define titor_is_valid(t) (mitor_is_valid((t).m) && ((t).m.data->content == GC_PURE || (t).tran != NULL))
void titor_next(Titor *titor);
#define titor_get_row(t) mitor_get_row((t).m)
#define titor_get_row_size(t) mitor_get_row_size((t).m)
#define titor_get_cell(t, c) mitor_get_cell((t).m, c)
#define titor_get_datum(t, c) mitor_get_datum((t).m, c)
#define titor_put_datum(t, c, datum) mitor_put_datum((t).m, c, datum)

/** @note The following constants can be calculated on the fly
 *          but are intruduced here for performance issues, which means that
 *          tmin and tmax should open mvcc tables */
#define TMIN_IDX    0   /**< tmax clumn index in mvcc table */
#define TMAX_IDX    1   /**< tmax clumn index in mvcc table */

/**
 * @note We don't implement function table_init as we have to know if
 * the table is header or data right when initialization, use
 * htable_init or dtable_init instead.
 */

 /* @brief Allocates a new row in the Mesh.
 *
 * @param tran      Transaction
 * @param tail      pointer to tail gid of the Mesh. If row is allocated in
 *                  a new grid, tail data gid is changed to the new grid's
 *                  gid.
 * @return          Mitor of the new allocated row
 */
void table_alloc_row(Titor *titor, GidPair *tail);

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

/**
 * @brief Retrieves a column definition from a header table.
 *
 * @param grid      Pointer to the header grid
 * @param n         Column index (0-based)
 * @return          Pointer to the Column structure at the specified index
 */
#define htable_get_column(grid, n) hmesh_get_column(grid, n)

#define htable_get_column_idx(grid, name) hmesh_get_column_idx(grid, name)

Grid *htable_init(Page page, uint16_t pagesz, GridLayout layout, GridContent content);

Grid *dtable_init(Page page, uint16_t pagesz, GridLayout layout, GridContent content, Grid *hgrid);

#define table_get_cell(itor, column) mesh_get_cell(itor, column)

#endif /* _TABLE_H_ */

