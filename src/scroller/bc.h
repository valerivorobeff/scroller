#ifndef _BC_H_
#define _BC_H_

#include "execute.y.h"
#include <stddef.h>

typedef struct BcNode {
    int token;
    Y2STYPE value;
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
int y2lex(Y2STYPE *yylval, Bc *bc);

#endif /* _BC_H_ */

