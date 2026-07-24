#ifndef _CMD_H_
#define _CMD_H_

#include "hquery.y.h"

typedef struct Context Context;
typedef struct Server Server;

typedef struct BcNode {
    int token;
    YYSTYPE value;
} BcNode;

typedef struct Cmd {
    Context *bc_cont;
    Context *str_cont;
    Server *server;
    BcNode *bc;
} Cmd;

Cmd *cmd_init(Cmd *cmd, Server *server);
void cmd_drop(Cmd *cmd);
void cmd_reset(Cmd *cmd);

#endif /* _CMD_H_ */

