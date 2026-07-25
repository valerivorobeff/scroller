%code requires{
typedef struct BcNode BcNode;
typedef struct Cmd Cmd;
}

%code {
#include "hquery.y.h"
#include "hquery.l.h"
#include "array.h"
#include "memory.h"
#include "../../../../src/scroller/cmd.h"
#include "../../../../src/scroller/server.h"
#include "../../../../src/scroller/flog.h"
#include <sys/socket.h>

void yyerror(yyscan_t scanner, Cmd *cmd, char const *s);
}

%define api.pure full
%define api.prefix {y1}
 //%locations
%lex-param      {yyscan_t scanner}
%parse-param    {void *scanner}
%parse-param    {Cmd *cmd}

/* @todo: write the functions */
 //%initial-action {}
 //%destructor { } <>

%union {
    char* str;
}

/* Common tokens */
%token USER
%token <str> STRING

/* Header tokens */
%token HEADER_END
%token REQUEST_END

/* Body tokens */
%token CREATE
%token INSERT INTO VALUES

%%

session:
    request
    |
    request session
    ;

request:
    header REQUEST_END {
        /* @todo: here we should reset header or request memory context
            but at the moment we don't have it, we should make it */
        const char *response = "Status: Empty\n\n";
        send(cmd->server->client_fd, response, strlen(response), 0);
        flog("Status Empty");
        flog_flush();
    }
    |
    header body REQUEST_END {
        /* @todo: here we should reset header or request memory context
            but at the moment we don't have it, we should make it */
        const char *response = "Status: Ready\n\n";
        send(cmd->server->client_fd, response, strlen(response), 0);
        flog("Status Ready");
        flog_flush();
    }
    ;

header:
    header_exprs HEADER_END
    ;

header_exprs:
    header_expr
    |
    header_exprs header_expr
    ;

header_expr:
    USER ':' STRING '\n'
    ;

body:
    cmd ';' { y2parse(&cmd->bc); cmd_reset(cmd); }
    |
    body cmd ';' { y2parse(&cmd->bc); cmd_reset(cmd); }
    ;

cmd:
    CREATE USER STRING {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_CREATE }));
        bc_put(&cmd->bc, ((BcNode){ .token = BC_USER }));
        bc_put(&cmd->bc, ((BcNode){ .token = BC_STRING }));

    }
    |
    INSERT INTO STRING {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_INSERT }));
        bc_put(&cmd->bc, ((BcNode){ .token = BC_STRING, .value.str = $3 }));
        bc_put(&cmd->bc, ((BcNode){ .token = BC_ARRAY_BEGIN }));
    } '(' strings ')' {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_ARRAY_END }));
    } VALUES {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_ARRAY_BEGIN }));
    } '(' strings ')' {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_ARRAY_END }));
        flog("INSERT INTO %s", $3);
        flog_flush();
    }
    ;

strings:
    STRING {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_STRING, .value.str = $1 }));
        flog("%s", $1);
    }
    |
    strings ',' STRING {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_STRING, .value.str = $3 }));
        flog("%s", $3);
    }
    ;

%%

/* Called by yyparse on error. */
void
yyerror(yyscan_t scanner, Cmd *cmd, char const *s) {
    (void)scanner;
    (void)cmd;
    ferr("%s\n", s);
}

