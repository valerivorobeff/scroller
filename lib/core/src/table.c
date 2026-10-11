#include "table.h"
#include "tran.h"
#include <assert.h>

Titor titor_init(Tran *tran, GidPair gp, Grid *header, Grid *data);
void titor_next(Titor *titor);
Grid *htable_init(Page page, uint16_t pagesz, GridLayout layout, GridContent content);
Grid *dtable_init(Page page, uint16_t pagesz, GridLayout layout, GridContent content, Grid *hgrid);
void table_alloc_row(Titor *titor, GidPair *tail);
Column *htable_add_column(Grid *grid, const char *name, Type type, size_t size);

Titor
titor_init(Tran *tran, GidPair gp, Grid *header, Grid *data) {
    Titor ret = { .m = mitor_init(gp, header, data), .tran = tran };

    switch (data->content) {
        case GC_PURE:
            break;

        case GC_MVCC:
            Datum tmin = titor_get_datum(ret, TMIN_IDX);
            Datum tmax = titor_get_datum(ret, TMAX_IDX);

            assert(tran != NULL);

            assert(hgrid_get_column_idx(header, "*tmin") == TMIN_IDX);
            assert(hgrid_get_column_idx(header, "*tmax") == TMAX_IDX);

            assert(tmin.type == T_TRANID);
            assert(tmax.type == T_TRANID);

            if (!titor_is_valid(ret))
                break;

            if (!tran_row_is_visible(tran, (tran_t)tmin.value.integer, (tran_t)tmax.value.integer))
                titor_next(&ret);

            break;
    }

    return ret;
}

void
titor_next(Titor *titor) {
    switch (titor->m.data->content) {
        case GC_PURE:
            mitor_next(&titor->m);
            break;

        case GC_MVCC:
            for (;;) {
                mitor_next(&titor->m);

                if (!mitor_is_valid(titor->m))
                    break;

                Datum tmin = mitor_get_datum(titor->m, TMIN_IDX);
                Datum tmax = mitor_get_datum(titor->m, TMAX_IDX);

                if (tran_row_is_visible(titor->tran, (tran_t)tmin.value.integer, (tran_t)tmax.value.integer))
                    break;
            }
            break;
    }
}

Grid *
htable_init(Page page, uint16_t pagesz, GridLayout layout, GridContent content) {
    Grid *ret = hmesh_init(page, pagesz, layout, content);

    if (content == GC_MVCC) {
        if (htable_add_column(ret, "*tmin", T_TRANID, 0) == NULL)
            return NULL;    /* SCRS_BAD_ALLOC */

        if (htable_add_column(ret, "*tmax", T_TRANID, 0) == NULL)
            return NULL;    /* SCRS_BAD_ALLOC */
    }

    /* Check if tmin and tmax open the row */
    assert(content == GC_PURE || hgrid_get_column_idx(ret, "*tmin") == TMIN_IDX);
    assert(content == GC_PURE || hgrid_get_column_idx(ret, "*tmax") == TMAX_IDX);

    return ret;
}

Grid *
dtable_init(Page page, uint16_t pagesz, GridLayout layout, GridContent content, Grid *hgrid) {
    return dmesh_init(page, pagesz, layout, content, hgrid);
}

/* @todo check if parameter tail can be fetched from parameter titor and
 * should not be parameter of this function */
/* @todo interfface of this function looks strange. It actually can return a ScrcStatus
 * basing on which mitor mesh_alloc_row returns */
void
table_alloc_row(Titor *titor, GidPair *tail) {
    titor->m = mesh_alloc_row(tail);
    titor->m.gp = *tail;
}

Column *
htable_add_column(Grid *grid, const char *name, Type type, size_t size) {
    return hmesh_add_column(grid, name, type, size);
}

