#include "mesh.h"
#include "pagecache.h"
#include "sequence.h"

extern PageCache *g_pagecache;

Mitor mesh_alloc_row(GidPair *tail);
Column *hmesh_add_column(Grid *grid, const char *name, Type type, size_t size);
void mitor_next(Mitor *mitor);

Mitor
mesh_alloc_row(GidPair *tail) {
    Grid *header = pagecache_put_page(g_pagecache, tail->header.full);
    Grid *data = pagecache_put_page(g_pagecache, tail->data.full);
    uint16_t row = grid_alloc_row(data);

    if (row == GRID_INVALID_IDX) {
        if (tail->data.parts.page < GID_MAXPAGE) {
            ++tail->data.parts.page;
        } else {
            /* @todo */
            /* Increment sequence */
            int64_t currval;
            Grid *hsequence = NULL;//pagecache_put_page(g_pagecache, g_server.system.sequence.header.full);
            Grid *sequence = NULL;//pagecache_put_page(g_pagecache, g_server.system.sequence.data.full);

            if (sequence_nextval(hsequence, sequence, &currval))
                return (Mitor) { NULL, NULL, GRID_INVALID_IDX }; /* SCRS_SEQUENCE_OVERFLOW */

            tail->data = (Gid) { .parts = { .file_id = currval, .page = 0 } };
        }

        data = pagecache_put_page(g_pagecache, tail->data.full);
        data = dgrid_init(data, PAGESZ, GT_FIXED, header);

        row = grid_alloc_row(data);
    }

    return (Mitor) { header, data, row };
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
                mitor->data = pagecache_put_page(g_pagecache, mitor->data->next.full);
                mitor->row = 0;
            }
        }
    }
}

