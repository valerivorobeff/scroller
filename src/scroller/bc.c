#include "bc.h"
#include "array.h"

Bc *bc_init(Bc *bc);
void bc_drop(Bc *bc);
void bc_clear(Bc *bc);
void bc_reset(Bc *bc);
void bc_put(Bc *bc, BcNode node);
int y2lex(Y2STYPE *yylval, Bc *bc);

Bc *
bc_init(Bc *bc) {
    bc->itor = 0;
    bc->tokens = NULL;

    return bc;
}

void
bc_drop(Bc *bc) {
    bc->itor = 0;
    array_free(bc->tokens);
}

void
bc_clear(Bc *bc) {
    bc->itor = 0;
    array_clear(bc->tokens);
}

void
bc_reset(Bc *bc) {
    bc->itor = 0;
}

void
bc_put(Bc *bc, BcNode node) {
    array_put(bc->tokens, node);
}

int
y2lex(Y2STYPE *yylval, Bc *bc) {
    if (bc->itor == array_size(bc->tokens)) {
        return 0;
    } else {
        *yylval = bc->tokens[bc->itor].value;
        return bc->tokens[bc->itor++].token;
    }
}

