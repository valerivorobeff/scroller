#ifndef _BC_H_
#define _BC_H_

#include "execute.y.h"
#include <stddef.h>

typedef struct Cmd Cmd;

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

/**
 * @brief Prepare bytecode for execution
 * Scans bytecode for LOOP_BEGIN/LOOP_END pairs and sets
 *       values for each.
 *
 * @note it should be called after the bc is filled in and before y2lex() is
 *       first called
 *
 * @param bc Bytecode
 * @return 0 - if succeed, 1 - if stack overflow, 2 - if unbalanced loop
 */
int bc_prepare(Bc *bc);

int y2lex(Y2STYPE *yylval, Cmd *cmd);

#endif /* _BC_H_ */

