/**
 * @file grid.h
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

/**
 * @todo:
 * 1. Make an allignment for rows.
 * 2. Make it an option to allocate new rows in strict order (like in vector)
 *      or not.
 * 3. Make it an option to store data in foxed or flexible size rows.
 */

#ifndef _GRID_H_
#define _GRID_H_

#include "cell.h"
#include "type.h"
#include <stddef.h>

/**
 * Global extern variable PAGESZ with default value 4096
 * You should better recalculate it at the beginning of your application
 */
extern size_t PAGESZ;

/**
 * @brief Opaque handle to a memory page.
 *
 * Points to a contiguous block of memory used as a storage page.
 */
typedef void *Page;

/**
 * @brief Opaque handle to a grid row.
 *
 * Represents a single row within a grid structure.
 */
typedef void *Row;

/**
 * @brief Grid type enumeration.
 */
typedef enum GridType : uint16_t {
    GT_FIXED = 1  /** Fixed-size grid with predetermined row size */
} GridType;

/**
 * @brief Main grid header structure.
 *
 * This structure precedes the actual data storage and contains metadata
 * about the grid layout, capacity, and usage.
 *
 * @note The datum[] flexible array member immediately follows this header
 *       and contains the actual row storage.
 */
typedef struct Grid {
    char        magic[4];    /** Magic number for validation (typically "scr ") */
    uint16_t    size;        /** Total size of the grid structure in bytes */
    GridType    type;        /** Type of grid (fixed, variable, etc.) */
    uint16_t    rowsz;       /** Size of each individual row in bytes */
    uint16_t    rown;        /** Maximum number of rows that can be stored */
    uint16_t    occupied;    /** Number of currently occupied rows */
    char        reserved[2]; /** Reserved for future use, must be zero */
    char        datum[];     /** Flexible array member containing row data */
} Grid;

/**
 * @brief Maximum length for column names including null terminator
 */
#define NAMESZ  16

/**
 * @brief Column definition structure for header grids.
 *
 * This structure defines a single column's properties within a header grid.
 * It maps column names to offsets and sizes within each data row.
 *
 * @see hgrid_add_column()
 * @see hgrid_get_column()
 */
typedef struct Column {
    char name[NAMESZ];   /** Column name, null-terminated string */
    size_t offs;         /** Byte offset of this column within a row */
    Type type;           /** Column type */
    size_t size;         /** Size of this column's data in bytes */
} Column;

#define GRID_INVALID_IDX    ((uint16_t)-1)

#define grid_idx_valid(idx) ((idx) != GRID_INVALID_IDX)

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
Grid *grid_init(Page page, uint16_t pagesz, GridType type, uint16_t rowsz);

/**
 * @brief Retrieves a pointer to a specific row in the grid.
 *
 * @param grid      Pointer to the grid structure
 * @param n         Row index (0-based)
 * @return          Pointer to the requested row, or NULL if index is out of range
 *
 * @warning The returned pointer becomes invalid if the grid is reallocated or destroyed.
 */
Row grid_get_row(Grid *grid, uint16_t n);

/**
 * @brief Retrieves a pointer to a specific cell within a data grid.
 *
 * @param hgrid     Header grid containing column definitions
 * @param grid      Data grid containing the actual row data
 * @param row       Row index within the data grid (0-based)
 * @param column    Column index (0-based) as defined in the header grid
 * @return          Pointer to the requested cell, or NULL if indices are invalid
 *
 * @note This function uses the Column definitions from hgrid to calculate
 *       the exact offset within the data row.
 */
Cell grid_get_cell(Grid *hgrid, Grid *grid, uint16_t row, uint16_t column);

/**
 * @brief Retrieves a Datum from a specific cell within a data grid.
 *
 * @param hgrid     Header grid containing column definitions
 * @param grid      Data grid containing the actual row data
 * @param row       Row index within the data grid (0-based)
 * @param column    Column index (0-based) as defined in the header grid
 * @return          Datum from the requested cell
 *
 * @note This function uses the Column definitions from hgrid to calculate
 *       the exact offset within the data row.
 */
Datum grid_get_datum(Grid *hgrid, Grid *grid, uint16_t row, uint16_t column);

/**
 * @brief Puts a Datum to a specific cell within a data grid.
 *
 * @param hgrid     Header grid containing column definitions
 * @param grid      Data grid containing the actual row data
 * @param row       Row index within the data grid (0-based)
 * @param column    Column index (0-based) as defined in the header grid
 * @param datum     Datum
 * @return          0 if sessess, error code otherwize
 *
 * @note This function uses the Column definitions from hgrid to calculate
 *       the exact offset within the data row.
 */
int grid_put_datum(Grid *hgrid, Grid *grid, uint16_t row, uint16_t column, Datum datum);

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
uint16_t grid_alloc_row(Grid *grid);

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
Column *hgrid_add_column(Grid *grid, const char *name, Type type, size_t size);

/**
 * @brief Calculates the total row size needed for a data grid.
 *
 * Sums up the sizes of all columns defined in the header grid.
 *
 * @param grid      Pointer to the header grid containing column definitions
 * @return          Total row size in bytes required to store all columns
 *
 * @note This value should be used as the rowsz parameter when initializing
 *       a data grid with dgrid_init().
 */
size_t hgrid_get_row_size(Grid *grid);

/**
 * @brief Returns column index by its name.
 *
 * @param grid      Pointer to the header grid containing column definitions
 * @param name      Column name.
 * @return          Column index.
 *
 * @note This value should be used as the rowsz parameter when initializing
 *       a data grid with dgrid_init().
 */
uint16_t hgrid_get_column_idx(Grid *grid, const char *name);

/**
 * @brief Initializes a header grid within a memory page.
 *
 * A header grid stores Column structures that define the schema for data grids.
 *
 * @param page      Pointer to the memory page for the header grid
 * @param pagesz    Size of the memory page in bytes
 * @param type      Grid type (typically GT_FIXED)
 * @return          Pointer to the initialized Grid structure
 *
 * @note This macro automatically sets the row size to sizeof(Column).
 * @see grid_init()
 */
#define hgrid_init(page, pagesz, type) \
    grid_init(page, pagesz, type, sizeof(Column))

/**
 * @brief Retrieves a column definition from a header grid.
 *
 * @param grid      Pointer to the header grid
 * @param n         Column index (0-based)
 * @return          Pointer to the Column structure at the specified index
 *
 * @note This macro casts the row pointer to Column* for convenience.
 * @see grid_get_row()
 */
#define hgrid_get_column(grid, n) ((Column *)grid_get_row(grid, n))

/**
 * @brief Initializes a data grid within a memory page.
 *
 * A data grid stores actual row data according to the schema defined in a header grid.
 *
 * @param page      Pointer to the memory page for the data grid
 * @param pagesz    Size of the memory page in bytes
 * @param type      Grid type (typically GT_FIXED)
 * @param hgrid     Header grid defining the column schema
 * @return          Pointer to the initialized Grid structure
 *
 * @note This macro automatically calculates row size by calling hgrid_get_row_size(hgrid).
 * @see grid_init()
 * @see hgrid_get_row_size()
 */
#define dgrid_init(page, pagesz, type, hgrid) \
    grid_init(page, pagesz, type, hgrid_get_row_size(hgrid))

/**
 * @brief Allocates a new row in a data grid.
 *
 * @param grid      Pointer to the data grid
 * @return          Newly allocated row index or GRID_INVALID_IDX if grid is full
 *
 * @note This macro is an alias for grid_alloc_row() for API symmetry.
 */
#define dgrid_alloc_row(grid) grid_alloc_row(grid)

/**
 * @brief Retrieves a cell from a data grid using header definitions.
 *
 * @param hgrid     Header grid containing column definitions
 * @param grid      Data grid containing the actual row data
 * @param row       Row index within the data grid
 * @param column    Column index (0-based) as defined in the header grid
 * @return          Pointer to the requested cell
 *
 * @note This macro is an alias for grid_get_column() for API symmetry.
 */
#define dgrid_get_cell(hgrid, grid, row, column) \
    grid_get_cell(hgrid, grid, row, column)

/**
 * @brief Retrieves a Datum from a specific cell within a data grid.
 *
 * @param hgrid     Header grid containing column definitions
 * @param grid      Data grid containing the actual row data
 * @param row       Row index within the data grid (0-based)
 * @param column    Column index (0-based) as defined in the header grid
 * @return          Datum from the requested cell
 *
 * @note This function uses the Column definitions from hgrid to calculate
 *       the exact offset within the data row.
 */
#define dgrid_get_datum(hgrid, grid, row, column) \
    grid_get_datum(hgrid, grid, row, column)

/**
 * @brief Puts a Datum to a specific cell within a data grid.
 *
 * @param hgrid     Header grid containing column definitions
 * @param grid      Data grid containing the actual row data
 * @param row       Row index within the data grid (0-based)
 * @param column    Column index (0-based) as defined in the header grid
 * @param datum     Datum
 * @return          0 if sessess, error code otherwize
 *
 * @note This function uses the Column definitions from hgrid to calculate
 *       the exact offset within the data row.
 */
#define dgrid_put_datum(hgrid, grid, row, column, datum) \
    grid_put_datum(hgrid, grid, row, column, datum)

#endif /* _GRID_H_ */

