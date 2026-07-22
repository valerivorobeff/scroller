#include "cmd.h"
#include "memory.h"
#include "array.h"

#define BCSZ 1024

Cmd *
cmd_init(Cmd *cmd, Server *server) {
    Context *prev;

                                            /* Create separate context for bytecode */
    cmd->bc_cont = linear_context_create(sizeof(BcNode) * BCSZ);

                                            /* Create separate context for string values */
    cmd->str_cont = linear_context_create(sizeof(const char *) * BCSZ);

    cmd->server = server;

    prev = context_switch(cmd->bc_cont);    /* switch to bytecode context */
    cmd->bc = array_create(cmd->bc, BCSZ);  /* Create bytecode */
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

