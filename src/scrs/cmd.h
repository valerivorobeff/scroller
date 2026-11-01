#ifndef _CMD_H_
#define _CMD_H_

#include "bc.h"
#include "table.h"
#include "memory.h"

typedef struct Context Context;

typedef struct Cmd {
    Context *bc_cont;       /**< Context for bytecode */
    Context *str_cont;      /**< Context for strings */
    Context *current_cont;  /**< Context for current dynamic operations */
    Bc bc;                  /**< Bytecode */
    Titor titor;            /**< Titor of current row in where expressions */
    void *current;          /**< Pointer to current data to use by parsers */
} Cmd;

Cmd *cmd_init(Cmd *cmd);
void cmd_drop(Cmd *cmd);
void cmd_reset(Cmd *cmd);

#define cmd_current_put(cmd, a, v) \
    context_push(cmd->current_cont); \
    array_put(a, v); \
    context_pop();

#endif /* _CMD_H_ */

