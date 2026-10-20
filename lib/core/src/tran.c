#include "tran.h"
#include "ihash.h"
#include "pagecache.h"
#include "../../../src/scrs/server.h"
#include <stddef.h>
#include <assert.h>

extern Server g_server;

bool
tran_row_is_visible(const Tran *tran, tran_t tmin, tran_t tmax) {
    const tran_t tid = tran->key;

    assert(tid != TRAN_INVALID_ID);
    assert(tmin != TRAN_INVALID_ID);

    if (tid == tmin) {
        assert(tid == tmax || tmax == TRAN_INVALID_ID);
        return tmax == TRAN_INVALID_ID;
    } else if (tid == tmax) {
        return false;
    } else {
        /* Is tmin in progress? */
        if (ihash_get(g_tran, tmin) != NULL) {
            return false;
        }

        /* Is tmax in progress? */
        if (ihash_get(g_tran, tmax) != NULL) {
            return true;
        }

        /* Is tmin aborted? */
        if (tran_flag_get(tmin) == TF_ABORTED) {
            return false;
        }

        /* Is tmax commited? */
        return tran_flag_get(tmax) == TF_COMMITED;
    }
}

TranFlag
tran_flag_get(tran_t tid) {
    TranPage *page = pagecache_put_page(g_pagecache, g_server.system.tran);

    assert(page);

    tid -= page->tid_start;
//return TF_COMMITED;
//    assert (tid > 0);
    return (page->datum[tid / 8] >> (tid % 8)) & 1;
}

void
tran_flag_put(tran_t tid, TranFlag flag) {
    TranPage *page = pagecache_put_page(g_pagecache, g_server.system.tran);

    assert(page);

    tid -= page->tid_start;
//    assert (tid > 0);

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

