#ifndef _CMD_H_
#define _CMD_H_

#include "bc.h"
#include "table.h"

typedef struct Context Context;

typedef struct Cmd {
    Context *bc_cont;       /**< COntext for bytecode */
    Context *str_cont;      /**< Context for strings */
    Bc bc;                  /**< Bytecode */
    Titor titor;            /**< Titor of current row in where expressions */
    void *current;          /**< Pointer to current data to use by parsers */
} Cmd;

Cmd *cmd_init(Cmd *cmd);
void cmd_drop(Cmd *cmd);
void cmd_reset(Cmd *cmd);

#endif /* _CMD_H_ */

