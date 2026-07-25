#include "table.h"

Titor
titor_init(Grid *header, Grid *root, Grid *current, Row row) {
    return (Titor){ header, root, current, row };
}

