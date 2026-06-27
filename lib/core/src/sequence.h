/**
 * @file sequence.h
 * @brief Sequence
 *
 */

#ifndef _SEQUENCE_H_
#define _SEQUENCE_H_

#include "grid.h"
#include <stdbool.h>

int hsequence_init(Page page);

int sequence_init(Grid *hsequence, Page p, int64_t minval, int64_t maxval, int64_t startval, int64_t increment, bool cycle);
int sequence_currval(Grid *hsequence, Grid *sequence, int64_t *outval);
int sequence_nextval(Grid *hsequence, Grid *sequence, int64_t *outval);
int sequence_setval(Grid *hsequence, Grid *sequence, int64_t newval, bool is_called);
int sequence_set_minval(Grid *hsequence, Grid *sequence, int64_t val);
int sequence_set_maxval(Grid *hsequence, Grid *sequence, int64_t val);
int sequence_set_increment(Grid *hsequence, Grid *sequence, int64_t val);
int sequence_set_cycle(Grid *hsequence, Grid *sequence, bool val);


#endif /* _SEQUENCE_H_ */

