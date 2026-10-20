#include "tran.h"
#include "ihash.h"
#include "pagecache.h"
#include "../../../src/scrs/server.h"
#include <stddef.h>
#include <assert.h>

bool
tran_row_is_visible(const Tran *tran, tran_t tmin, tran_t tmax) {
    const tran_t tid = tran->key;

    assert(tid != TRAN_INVALID_ID);
    assert(tmin != TRAN_INVALID_ID);

    if (tid == tmin) {
        /* We inserted this row */
        assert(tid == tmax || tmax == TRAN_INVALID_ID);
        return tid != tmax;
    } else if (tid == tmax) {
        /* We deleted this row */
        return false;
    } else {
        /* Another transaction inserted and/or deleted this row */

        const TranFlag tmin_flag = tran_flag_get(tmin);
        const TranFlag tmax_flag = tran_flag_get(tmax);

        /* Is tmin commited? */
        if (tmin_flag == TF_COMMITED) {
            return tmax_flag != TF_COMMITED;
        } else {
            return false;
        }
    }
}

/* @todo check if tid overflows page size and increase it */
TranFlag
tran_flag_get(tran_t tid) {
    TranPage *page = pagecache_put_page(g_pagecache, g_server.system.tran);

    assert(page);

    tid -= page->tid_start;
    assert (tid >= 0);

    return (page->datum[tid / 8] >> (tid % 8)) & 1;
}

/* @todo check if tid overflows page size and increase it */
void
tran_flag_put(tran_t tid, TranFlag flag) {
    TranPage *page = pagecache_put_page(g_pagecache, g_server.system.tran);

    assert(page);

    tid -= page->tid_start;
    assert (tid > 0);

    switch (flag) {
        case TF_ABORTED:
            page->datum[tid / 8] &= ~(1 << (tid % 8));
            break;

        case TF_COMMITED:
            page->datum[tid / 8] |= 1 << (tid % 8);
            break;
    }

    pagecache_flush(g_pagecache, g_server.system.tran);
}

