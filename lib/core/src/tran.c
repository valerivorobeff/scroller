#include "tran.h"
#include "ihash.h"
#include <stddef.h>
#include <assert.h>

extern Tran *g_tran;

bool
tran_row_is_visible(const Tran *tran, tran_t tmin, tran_t tmax) {
    const tran_t tid = tran->key;

    assert(tmin != TRAN_INVALID_ID);

    if (tid == tmin) {
        assert(tid == tmax || tmax == TRAN_INVALID_ID);
        return tmax == TRAN_INVALID_ID;
    } else {
        const Tran *tran_max = ihash_get(g_tran, tmax);

        if (tmax == TRAN_INVALID_ID)
            return tran_max == NULL;
        else
            return tran_max != NULL;
    }
}

