/**
 * @file sequence.h
 * @brief Sequence generator for SQL-like sequences
 *
 * Provides functionality for creating and managing sequences with
 * min/max bounds, increment steps, and cycle support.
 */

#ifndef _SEQUENCE_H_
#define _SEQUENCE_H_

#include "grid.h"
#include <stdbool.h>

/**
 * @brief Initialize sequence header grid
 * @param page Page for header storage
 * @return 0 on success, non-zero on error
 * @note Called only from scr_init utility
 */
int hsequence_init(Page page);

/**
 * @brief Create a new sequence
 * @param hsequence Header grid (created by hsequence_init)
 * @param p Page for sequence data
 * @param minval Minimum value (inclusive)
 * @param maxval Maximum value (inclusive)
 * @param startval Starting value
 * @param increment Step value (cannot be 0)
 * @param cycle Whether to cycle on overflow
 * @return 0 on success, 1 on error
 */
int sequence_init(Grid *hsequence, Page p, int64_t minval, int64_t maxval,
                  int64_t startval, int64_t increment, bool cycle);

/**
 * @brief Get current value without advancing
 * @param hsequence Header grid
 * @param sequence Sequence grid
 * @param outval Output: current value
 * @return 0 on success, 1 if not called yet
 */
int sequence_currval(Grid *hsequence, Grid *sequence, int64_t *outval);

/**
 * @brief Advance sequence and return next value
 * @param hsequence Header grid
 * @param sequence Sequence grid
 * @param outval Output: next value
 * @return 0 on success, 1 if bounds exceeded (no cycle)
 */
int sequence_nextval(Grid *hsequence, Grid *sequence, int64_t *outval);

/**
 * @brief Set current value
 * @param hsequence Header grid
 * @param sequence Sequence grid
 * @param newval New current value
 * @param is_called Mark sequence as called
 * @return 0 on success, 1 if value out of bounds
 */
int sequence_setval(Grid *hsequence, Grid *sequence, int64_t newval, bool is_called);

/**
 * @brief Set minimum value
 * @param hsequence Header grid
 * @param sequence Sequence grid
 * @param val New minimum value
 * @return 0 on success, 1 if val >= maxval or val > current
 */
int sequence_set_minval(Grid *hsequence, Grid *sequence, int64_t val);

/**
 * @brief Set maximum value
 * @param hsequence Header grid
 * @param sequence Sequence grid
 * @param val New maximum value
 * @return 0 on success, 1 if val <= minval or val < current
 */
int sequence_set_maxval(Grid *hsequence, Grid *sequence, int64_t val);

/**
 * @brief Set increment step
 * @param hsequence Header grid
 * @param sequence Sequence grid
 * @param val New increment (cannot be 0)
 * @return 0 on success, 1 if val == 0
 */
int sequence_set_increment(Grid *hsequence, Grid *sequence, int64_t val);

/**
 * @brief Set cycle flag
 * @param hsequence Header grid
 * @param sequence Sequence grid
 * @param val New cycle flag
 * @return 0 on success, 1 on error
 */
int sequence_set_cycle(Grid *hsequence, Grid *sequence, bool val);

#endif /* _SEQUENCE_H_ */
