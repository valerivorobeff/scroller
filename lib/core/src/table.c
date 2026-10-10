#include "table.h"
#include <assert.h>

Grid *htable_init(Page page, uint16_t pagesz, GridLayout layout, GridContent content);
Grid *dtable_init(Page page, uint16_t pagesz, GridLayout layout, GridContent content, Grid *hgrid);
Titor table_alloc_row(GidPair *tail);
Column *htable_add_column(Grid *grid, const char *name, Type type, size_t size);

Grid *
htable_init(Page page, uint16_t pagesz, GridLayout layout, GridContent content) {
    Grid *ret = hmesh_init(page, pagesz, layout, content);

    if (content == GC_MVCC) {
        if (htable_add_column(ret, "*tmin", T_BIGINT, 0) == NULL)
            return NULL;    /* SCRS_BAD_ALLOC */

        if (htable_add_column(ret, "*tmax", T_BIGINT, 0) == NULL)
            return NULL;    /* SCRS_BAD_ALLOC */
    }

    return ret;
}

/* @todo actually we can get rid of parameter 'content' here because it must always be equal to
 * hgrid->content. On the other hand we let developer confirm it here */
Grid *
dtable_init(Page page, uint16_t pagesz, GridLayout layout, GridContent content, Grid *hgrid) {
    assert(content == hgrid->content);

    return dmesh_init(page, pagesz, layout, content, hgrid);
}

Titor
table_alloc_row(GidPair *tail) {
    return mesh_alloc_row(tail);
}

Column *
htable_add_column(Grid *grid, const char *name, Type type, size_t size) {
    return hmesh_add_column(grid, name, type, size);
}

