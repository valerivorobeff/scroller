#include "bc.h"
#include "cmd.h"
#include "array.h"

Bc *bc_init(Bc *bc);
void bc_drop(Bc *bc);
void bc_clear(Bc *bc);
void bc_reset(Bc *bc);
void bc_put(Bc *bc, BcNode node);
int bc_prepare(Bc *bc);
int y2lex(Y2STYPE *yylval, Cmd *cmd);

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
bc_prepare(Bc *bc) {
    /* Stack of WHERE_BEGIN indices */
    static const size_t STACKSZ = 64;
    size_t stack[STACKSZ];
    size_t sp = 0;

    for (size_t i = 0; i < array_size(bc->tokens); ++i) {
        BcNode *node = &bc->tokens[i];

        if (node->token == BC_LOOP_BEGIN) {
            if (sp == STACKSZ)
                return 1;                   /* Stack overflow */
            else
                stack[sp++] = i;            /* Push index to stack */
        } else if (node->token == BC_LOOP_END) {
            if (sp == 0)
                return 2;                   /* Unbalanced — error */

            const size_t begin_idx = stack[--sp];

            /* WHERE_BEGIN.integer = index after WHERE_END */
            bc->tokens[begin_idx].value.integer = i + 1;

            /* WHERE_END.integer = index of WHERE_BEGIN */
            node->value.integer = begin_idx;
        }
    }

    return 0;
}

int
y2lex(Y2STYPE *yylval, Cmd *cmd) {
    Bc *bc = &cmd->bc;

    if (bc->itor == array_size(bc->tokens)) {
        return 0;
    } else {
        int token = bc->tokens[bc->itor].token;

        if (token == BC_LOOP_BEGIN) {
            if (titor_is_valid(cmd->titor)) {
                ++bc->itor;
                if (bc->itor == array_size(bc->tokens))
                    return 0;
                else
                    token = bc->tokens[bc->itor].token;
            } else {
                /* Move forward past WHERE_END */
                bc->itor = bc->tokens[bc->itor].value.integer;
                if (bc->itor == array_size(bc->tokens))
                    return 0;

                token = bc->tokens[bc->itor].token;
            }

        } else if (token == BC_LOOP_END) {
            if (titor_is_valid(cmd->titor)) {
                bc->itor = bc->tokens[bc->itor].value.integer;
                token = bc->tokens[++bc->itor].token;
            } else {
                /* Move forward past WHERE_END */
                ++bc->itor;
                if (bc->itor == array_size(bc->tokens))
                    return 0;
                else
                    token = bc->tokens[bc->itor].token;
            }
        }

        *yylval = bc->tokens[bc->itor].value;
        ++bc->itor;

        return token;
    }
}

