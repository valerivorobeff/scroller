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
    context_add_child(context_get_current(), cmd->bc_cont); /* Child it to session context */

                                            /* Create separate context for string values */
    cmd->str_cont = linear_context_create(sizeof(const char *) * BCSZ * 2);
    assert(cmd->str_cont);
    context_add_child(context_get_current(), cmd->str_cont); /* Child it to session context */

    cmd->server = server;

    prev = context_switch(cmd->bc_cont);    /* switch to bytecode context */
    bc_init(&cmd->bc);                      /* Initialize bytecode */

    context_switch(prev);                   /* Switch back */

    return cmd;
}

void cmd_drop(Cmd *cmd) {
    /* We don't free cmd->bc here as it will be freed together with its memory context */
    context_drop(cmd->str_cont);
    context_drop(cmd->bc_cont);
}

void cmd_reset(Cmd *cmd) {
    /* We don't free cmd->bc here as it will be freed together with its memory context */
    context_reset(cmd->bc_cont);
    context_reset(cmd->str_cont);
}

