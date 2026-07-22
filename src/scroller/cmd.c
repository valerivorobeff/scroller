#include "cmd.h"
#include "memory.h"
#include "array.h"
#include <assert.h>

#define BCSZ 1024

Cmd *
cmd_init(Cmd *cmd, Server *server) {
    Context *prev;

    assert(cmd);

                                            /* Create separate context for bytecode */
    cmd->bc_cont = linear_context_create(sizeof(BcNode) * BCSZ * 2); /* @todo: calculate size of
                                                                        memory contexts depending on
                                                                        size of bc array */
    assert(cmd->bc_cont);

                                            /* Create separate context for string values */
    cmd->str_cont = linear_context_create(sizeof(const char *) * BCSZ * 2);
    assert(cmd->str_cont);

    cmd->server = server;

    prev = context_switch(cmd->bc_cont);    /* switch to bytecode context */
    cmd->bc = array_create(cmd->bc, BCSZ);  /* Create bytecode */
    assert(cmd->bc);

    context_switch(prev);                   /* Switch back */
    cmd->current = NULL;

    return cmd;
}

void cmd_drop(Cmd *cmd) {
    context_drop(cmd->str_cont);
    context_drop(cmd->bc_cont);
}

void cmd_reset(Cmd *cmd) {
    context_reset(cmd->bc_cont);
    context_reset(cmd->str_cont);
}

