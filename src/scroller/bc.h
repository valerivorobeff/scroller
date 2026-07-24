#ifndef _BC_H_
#define _BC_H_

#include "hquery.y.h"
#include <stddef.h>

typedef struct BcNode {
    int token;
    YYSTYPE value;
} BcNode;

typedef struct Bc {
    size_t itor;
    BcNode *tokens;
} Bc;

Bc *bc_init(Bc *bc);
void bc_drop(Bc *bc);
void bc_clear(Bc *bc);
void bc_reset(Bc *bc);
void bc_put(Bc *bc, BcNode node);
int y2lex(Bc *bc);

#endif /* _BC_H_ */

