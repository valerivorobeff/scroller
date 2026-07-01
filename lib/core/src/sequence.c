/**
 * @file sequence.c
 * @brief Sequence implementation
 */

#include "sequence.h"
#include <assert.h>

/**
 * @cond INTERNAL
 * Helper macros for reading/writing typed values from columns
 * @endcond
 */
#define put_smallint(c, val)    *(int16_t *)(c) = val
#define put_integer(c, val)     *(int32_t *)(c) = val
#define put_bigint(c, val)      *(int64_t *)(c) = val

#define get_smallint(c)         *(const int16_t *)(c)
#define get_integer(c)          *(const int32_t *)(c)
#define get_bigint(c)           *(const int64_t *)(c)

/* Forward declarations */
int hsequence_init(Page page);
int sequence_init(Grid *hsequence, Page p, int64_t minval, int64_t maxval,
                  int64_t startval, int64_t increment, bool cycle);
int sequence_currval(Grid *hsequence, Grid *sequence, int64_t *outval);
int sequence_nextval(Grid *hsequence, Grid *sequence, int64_t *outval);
int sequence_setval(Grid *hsequence, Grid *sequence, int64_t newval, bool is_called);
int sequence_set_minval(Grid *hsequence, Grid *sequence, int64_t val);
int sequence_set_maxval(Grid *hsequence, Grid *sequence, int64_t val);
int sequence_set_increment(Grid *hsequence, Grid *sequence, int64_t val);
int sequence_set_cycle(Grid *hsequence, Grid *sequence, bool val);

/**
 * @brief Initialize sequence header grid
 * @param page Page for header storage
 * @return 0 on success, non-zero on error
 * @note Called only from scr_init utility to initialize sequence header grid,
 *       then page should be saved to disk
 */
int
hsequence_init(Page page) {
    Grid *hsequence = hgrid_init(page, PAGESZ, GT_FIXED);

    hgrid_add_column(hsequence, "min_val", sizeof(int64_t));
    hgrid_add_column(hsequence, "max_val", sizeof(int64_t));
    hgrid_add_column(hsequence, "last_val", sizeof(int64_t));
    hgrid_add_column(hsequence, "increment", sizeof(int64_t));
    hgrid_add_column(hsequence, "cycle", sizeof(int64_t));
    hgrid_add_column(hsequence, "is_called", sizeof(int64_t));

    return 0;
}

/**
 * @brief Create a new sequence
 * @param hsequence Header grid
 * @param p Page for sequence data
 * @param minval Minimum value (inclusive)
 * @param maxval Maximum value (inclusive)
 * @param startval Starting value
 * @param increment Step value (cannot be 0)
 * @param cycle Whether to cycle on overflow
 * @return 0 on success, 1 on error
 */
int
sequence_init(Grid *hsequence, Page p, int64_t minval, int64_t maxval,
              int64_t startval, int64_t increment, bool cycle) {
    Column c;
    Grid *sequence = p;

    if (minval >= maxval) {
        return 1;
    }

    if (startval < minval) {
        return 1;
    }

    if (startval > maxval) {
        return 1;
    }

    if (increment == 0) {
        return 1;
    }

    sequence = dgrid_init(p, PAGESZ, GT_FIXED, hsequence);
    dgrid_alloc_row(sequence);
    c = dgrid_get_column(hsequence, sequence, 0, 0);
    put_bigint(c, minval);

    c = dgrid_get_column(hsequence, sequence, 0, 1);
    put_bigint(c, maxval);

    c = dgrid_get_column(hsequence, sequence, 0, 2);
    put_bigint(c, startval);

    c = dgrid_get_column(hsequence, sequence, 0, 3);
    put_bigint(c, increment);

    c = dgrid_get_column(hsequence, sequence, 0, 4);
    put_bigint(c, cycle);

    c = dgrid_get_column(hsequence, sequence, 0, 5);
    put_bigint(c, 0);

    return 0;
}

/**
 * @brief Get current value without advancing
 * @param hsequence Header grid
 * @param sequence Sequence grid
 * @param outval Output: current value
 * @return 0 on success, 1 if sequence not called yet
 */
int
sequence_currval(Grid *hsequence, Grid *sequence, int64_t *outval) {
    const Column c_currval = dgrid_get_column(hsequence, sequence, 0, 2);
    const Column c_is_called = dgrid_get_column(hsequence, sequence, 0, 5);

    if (get_bigint(c_is_called)) {
        *outval = get_bigint(c_currval);
        return 0;
    } else
        return 1;
}

/**
 * @brief Advance sequence and return next value
 * @param hsequence Header grid
 * @param sequence Sequence grid
 * @param outval Output: next value
 * @return 0 on success, 1 if bounds exceeded (no cycle)
 */
int
sequence_nextval(Grid *hsequence, Grid *sequence, int64_t *outval) {
    const Column c_minval = dgrid_get_column(hsequence, sequence, 0, 0);
    const Column c_maxval = dgrid_get_column(hsequence, sequence, 0, 1);
    const Column c_currval = dgrid_get_column(hsequence, sequence, 0, 2);
    const Column c_increment = dgrid_get_column(hsequence, sequence, 0, 3);
    const Column c_cycle = dgrid_get_column(hsequence, sequence, 0, 4);
    const Column c_is_called = dgrid_get_column(hsequence, sequence, 0, 5);
    const int64_t minval = get_bigint(c_minval);
    const int64_t maxval = get_bigint(c_maxval);
    const int64_t current = get_bigint(c_currval);
    const int64_t increment = get_bigint(c_increment);
    const int64_t cycle = get_bigint(c_cycle);
    const int64_t is_called = get_bigint(c_is_called);

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

/**
 * @brief Set current value
 * @param hsequence Header grid
 * @param sequence Sequence grid
 * @param val New current value
 * @param is_called Mark sequence as called
 * @return 0 on success, 1 if value out of bounds
 */
int
sequence_setval(Grid *hsequence, Grid *sequence, int64_t val, bool is_called) {
    const Column c_minval = dgrid_get_column(hsequence, sequence, 0, 0);
    const Column c_maxval = dgrid_get_column(hsequence, sequence, 0, 1);
    const Column c_currval = dgrid_get_column(hsequence, sequence, 0, 2);
    const Column c_is_called = dgrid_get_column(hsequence, sequence, 0, 5);
    const int64_t minval = get_bigint(c_minval);
    const int64_t maxval = get_bigint(c_maxval);

    if (val > maxval)
        return 1;

    if (val < minval)
        return 1;

    put_bigint(c_currval, val);
    put_bigint(c_is_called, is_called);

    return 0;
}

/**
 * @brief Set minimum value
 * @param hsequence Header grid
 * @param sequence Sequence grid
 * @param val New minimum value
 * @return 0 on success, 1 if val >= maxval or val > current
 */
int
sequence_set_minval(Grid *hsequence, Grid *sequence, int64_t val) {
    const Column c_minval = dgrid_get_column(hsequence, sequence, 0, 0);
    const Column c_maxval = dgrid_get_column(hsequence, sequence, 0, 1);
    const Column c_currval = dgrid_get_column(hsequence, sequence, 0, 2);
    const int64_t maxval = get_bigint(c_maxval);
    const int64_t current = get_bigint(c_currval);

    if (val >= maxval)
        return 1;

    if (current < val)
        return 1;

    put_bigint(c_minval, val);

    return 0;
}

/**
 * @brief Set maximum value
 * @param hsequence Header grid
 * @param sequence Sequence grid
 * @param val New maximum value
 * @return 0 on success, 1 if val <= minval or val < current
 */
int
sequence_set_maxval(Grid *hsequence, Grid *sequence, int64_t val) {
    const Column c_minval = dgrid_get_column(hsequence, sequence, 0, 0);
    const Column c_maxval = dgrid_get_column(hsequence, sequence, 0, 1);
    const Column c_currval = dgrid_get_column(hsequence, sequence, 0, 2);
    const int64_t minval = get_bigint(c_minval);
    const int64_t current = get_bigint(c_currval);

    if (val <= minval)
        return 1;

    if (current > val)
        return 1;

    put_bigint(c_maxval, val);

    return 0;
}

/**
 * @brief Set increment step
 * @param hsequence Header grid
 * @param sequence Sequence grid
 * @param val New increment (cannot be 0)
 * @return 0 on success, 1 if val == 0
 */
int
sequence_set_increment(Grid *hsequence, Grid *sequence, int64_t val) {
    Column c_increment;

    if (val == 0)
        return 1;

    c_increment = dgrid_get_column(hsequence, sequence, 0, 3);

    put_bigint(c_increment, val);

    return 0;
}

/**
 * @brief Set cycle flag
 * @param hsequence Header grid
 * @param sequence Sequence grid
 * @param val New cycle flag
 * @return 0 on success, 1 on error
 */
int
sequence_set_cycle(Grid *hsequence, Grid *sequence, bool val) {
    const Column c_cycle = dgrid_get_column(hsequence, sequence, 0, 4);

    put_bigint(c_cycle, val);

    return 0;
}

