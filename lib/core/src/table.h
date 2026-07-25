#ifndef _TABLE_H_
#define _TABLE_H_

#include "grid.h"

typedef struct Titor {
    Grid *header;
    Grid *root;
    Grid *current;
    Row row;
} Titor;

Titor titor_init(Grid *header, Grid *root, Grid *current, Row row);

#endif /* _TABLE_H_ */

