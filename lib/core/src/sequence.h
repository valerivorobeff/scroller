/**
 * @file sequence.h
 * @brief Sequence
 *
 */

#ifndef _SEQUENCE_H_
#define _SEQUENCE_H_

#include "grid.h"

void hsequence_init();

void sequence_init(Grid *hsequence, Page p);
int64_t sequence_curval(Grid *hsequence, Grid *sequence);
int64_t sequence_nextval(Grid *hsequence, Grid *sequence);

#endif /* _SEQUENCE_H_ */

