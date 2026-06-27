#include "sequence.h"
#include <assert.h>

void hsequence_init();

int sequence_init(Grid *hsequence, Page p, int64_t minval, int64_t maxval, int64_t startval, int64_t increment, bool cycle, Grid **sequence);
int sequence_currval(Grid *hsequence, Grid *sequence, int64_t *outval);
int sequence_nextval(Grid *hsequence, Grid *sequence, int64_t *outval);
int sequence_setval(Grid *hsequence, Grid *sequence, int64_t newval, bool is_called);
int sequence_set_minval(Grid *hsequence, Grid *sequence, int64_t val);
int sequence_set_maxval(Grid *hsequence, Grid *sequence, int64_t val);
int sequence_set_increment(Grid *hsequence, Grid *sequence, int64_t val);
int sequence_set_cycle(Grid *hsequence, Grid *sequence, bool val);

#define put_smallint(c, val)    *(int16_t *)(c) = val
#define put_integer(c, val)     *(int32_t *)(c) = val
#define put_bigint(c, val)      *(int64_t *)(c) = val

#define get_smallint(c)         *(int16_t *)(c)
#define get_integer(c)          *(int32_t *)(c)
#define get_bigint(c)           *(int64_t *)(c)

void
hsequence_init(Page page) {
    Grid *hsequence = hgrid_init(page, PAGESZ, GT_FIXED);

    hgrid_add_column(hsequence, "min_val", sizeof(int64_t));
    hgrid_add_column(hsequence, "max_val", sizeof(int64_t));
    hgrid_add_column(hsequence, "last_val", sizeof(int64_t));
    hgrid_add_column(hsequence, "increment", sizeof(int64_t));
    hgrid_add_column(hsequence, "cycle", sizeof(int64_t));
    hgrid_add_column(hsequence, "is_called", sizeof(int64_t));
}

int
sequence_init(Grid *hsequence, Page p, int64_t minval, int64_t maxval, int64_t startval, int64_t increment, bool cycle, Grid **sequence) {
    Column c;

    if (minval >= maxval) {
        sequence = NULL;
        return 1;
    }

    if (startval < minval) {
        sequence  = NULL;
        return 1;
    }

    if (startval > maxval) {
        sequence  = NULL;
        return 1;
    }

    if (increment == 0) {
        sequence  = NULL;
        return 1;
    }

    *sequence = dgrid_init(p, PAGESZ, GT_FIXED, hsequence);
    c = dgrid_get_column(hsequence, *sequence, 0, 0);
    put_bigint(c, minval);

    c = dgrid_get_column(hsequence, *sequence, 0, 1);
    put_bigint(c, maxval);

    c = dgrid_get_column(hsequence, *sequence, 0, 2);
    put_bigint(c, startval);

    c = dgrid_get_column(hsequence, *sequence, 0, 3);
    put_bigint(c, increment);

    c = dgrid_get_column(hsequence, *sequence, 0, 4);
    put_bigint(c, cycle);

    c = dgrid_get_column(hsequence, *sequence, 0, 5);
    put_bigint(c, 0);

    return 0;
}

int
sequence_currval(Grid *hsequence, Grid *sequence, int64_t *outval) {
    Column c_currval = dgrid_get_column(hsequence, sequence, 0, 2);
    Column c_is_called = dgrid_get_column(hsequence, sequence, 0, 5);

    if (get_bigint(c_is_called)) {
        *outval = get_bigint(c_currval);
        return 0;
    } else
        return 1;
}

int
sequence_nextval(Grid *hsequence, Grid *sequence, int64_t *outval) {
    Column c_minval = dgrid_get_column(hsequence, sequence, 0, 0);
    Column c_maxval = dgrid_get_column(hsequence, sequence, 0, 1);
    Column c_currval = dgrid_get_column(hsequence, sequence, 0, 2);
    Column c_increment = dgrid_get_column(hsequence, sequence, 0, 3);
    Column c_cycle = dgrid_get_column(hsequence, sequence, 0, 4);
    Column c_is_called = dgrid_get_column(hsequence, sequence, 0, 5);
    int64_t minval = get_bigint(c_minval);
    int64_t maxval = get_bigint(c_maxval);
    int64_t current = get_bigint(c_currval);
    int64_t increment = get_bigint(c_increment);
    int64_t cycle = get_bigint(c_cycle);
    int64_t is_called = get_bigint(c_is_called);

    assert(increment != 0);

    if (is_called) {
        int64_t newval = current + increment;

        if (increment > 0) {
            /* sequence is progressive */
            if (newval > maxval) {
                if (cycle)
                    newval = minval;
                else
                    return 1;
            }
        } else {
            /* sequence is regressive */
            if (newval < minval) {
                if (cycle)
                    newval = maxval;
                else
                    return 1;
            }
        }

        put_bigint(c_currval, newval);
        *outval = newval;
    } else {
        put_bigint(c_is_called, 1);
        *outval = current;
    }

    return 0;
}

int
sequence_setval(Grid *hsequence, Grid *sequence, int64_t val, bool is_called) {
    Column c_minval = dgrid_get_column(hsequence, sequence, 0, 0);
    Column c_maxval = dgrid_get_column(hsequence, sequence, 0, 1);
    Column c_currval = dgrid_get_column(hsequence, sequence, 0, 2);
    Column c_is_called = dgrid_get_column(hsequence, sequence, 0, 5);
    int64_t minval = get_bigint(c_minval);
    int64_t maxval = get_bigint(c_maxval);

    if (val > maxval)
        return 1;

    if (val < minval)
        return 1;

    put_bigint(c_currval, val);
    put_bigint(c_is_called, is_called);

    return 0;
}

int
sequence_set_minval(Grid *hsequence, Grid *sequence, int64_t val) {
    Column c_minval = dgrid_get_column(hsequence, sequence, 0, 0);
    Column c_maxval = dgrid_get_column(hsequence, sequence, 0, 1);
    Column c_currval = dgrid_get_column(hsequence, sequence, 0, 2);
    int64_t maxval = get_bigint(c_maxval);
    int64_t current = get_bigint(c_currval);

    if (val >= maxval)
        return 1;

    if (current < val)
        return 1;

    put_bigint(c_minval, val);

    return 0;
}

int
sequence_set_maxval(Grid *hsequence, Grid *sequence, int64_t val) {
    Column c_minval = dgrid_get_column(hsequence, sequence, 0, 0);
    Column c_maxval = dgrid_get_column(hsequence, sequence, 0, 1);
    Column c_currval = dgrid_get_column(hsequence, sequence, 0, 2);
    int64_t minval = get_bigint(c_minval);
    int64_t current = get_bigint(c_currval);

    if (val <= minval)
        return 1;

    if (current > val)
        return 1;

    put_bigint(c_maxval, val);

    return 0;
}

int
sequence_set_increment(Grid *hsequence, Grid *sequence, int64_t val) {
    Column c_increment;

    if (val == 0)
        return 1;

    c_increment = dgrid_get_column(hsequence, sequence, 0, 3);

    put_bigint(c_increment, val);

    return 0;
}

int
sequence_set_cycle(Grid *hsequence, Grid *sequence, bool val) {
    Column c_cycle = dgrid_get_column(hsequence, sequence, 0, 4);

    put_bigint(c_cycle, val);

    return 0;
}

