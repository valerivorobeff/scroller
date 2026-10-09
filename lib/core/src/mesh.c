#include "mesh.h"
#include "pagecache.h"
#include "sequence.h"
#include <assert.h>

extern PageCache *g_pagecache;
extern GidPair g_sequence;

Mitor mesh_alloc_row(GidPair *tail);
Column *hmesh_add_column(Grid *grid, const char *name, Type type, size_t size);
void mitor_next(Mitor *mitor);
void mitor_delete(Mitor *mitor);

Mitor
mesh_alloc_row(GidPair *tail) {
    Grid *header = pagecache_put_page(g_pagecache, tail->header);
    Grid *data = pagecache_put_page(g_pagecache, tail->data);
    uint16_t row = grid_alloc_row(data);

    assert(data->next.full == GID_UNDEF); /* Ensure this is really the tail gid */

    if (row == GRID_INVALID_IDX) {
        Grid *old_data = data;

        if (tail->data.parts.page < GID_MAXPAGE) {
            ++tail->data.parts.page;
        } else {
            /* File size is maximum allowed, get a new gid from sequence */
            /* Increment sequence */
            int64_t currval;
            Grid *hsequence = pagecache_put_page(g_pagecache, g_sequence.header);
            Grid *sequence = pagecache_put_page(g_pagecache, g_sequence.data);

            if (sequence_nextval(hsequence, sequence, &currval))
                return mitor_init_invalid(); /* SCRS_SEQUENCE_OVERFLOW */

            tail->data = (Gid) { .parts = { .file_id = currval, .page = 0 } };
        }

        /* Allocate a new grid in pagecache */
        /* @todo I should lock old table somehow, otherwise it can be removed from cache before I change its
         * content via old_data */
        data = pagecache_put_page(g_pagecache, tail->data);
        data = dgrid_init(data, PAGESZ, old_data->layout, old_data->content, header);

        old_data->next = tail->data;

        row = grid_alloc_row(data);
    }

    return (Mitor) { *tail, header, data, row };
}

Column *
hmesh_add_column(Grid *grid, const char *name, Type type, size_t size) {
    return hgrid_add_column(grid, name, type, size);
}

void
mitor_next(Mitor *mitor) {
    if (mitor_is_valid(*mitor)) {
        ++mitor->row;
        if (!mitor_is_valid(*mitor)) {
            if (mitor->data->next.full != GID_UNDEF) {
                mitor->gp.data = mitor->data->next;
                mitor->data = pagecache_put_page(g_pagecache, mitor->data->next);
                mitor->row = 0;
            }
        }
    }
}

void
mitor_delete(Mitor *mitor) {
    uint16_t ret = grid_delete_row(mitor->data, mitor->row);
    assert(ret != GRID_INVALID_IDX);
}

