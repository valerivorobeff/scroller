%code requires{
typedef struct BcNode BcNode;
typedef struct Session Session;
typedef struct Cmd Cmd;
}

%code {
#include "hquery.y.h"
#include "hquery.l.h"
#include "array.h"
#include "memory.h"
#include "../../../../src/scroller/worker.h"
#include "../../../../src/scroller/cmd.h"
#include "../../../../src/scroller/flog.h"
#include <sys/socket.h>

void yyerror(yyscan_t scanner, Session *session, Cmd *cmd, char const *s);
}

%define api.pure full
%define api.prefix {y1}
 //%locations
%lex-param      {yyscan_t scanner}
%parse-param    {void *scanner}
%parse-param    {Session *session}
%parse-param    {Cmd *cmd}

/* @todo: write the functions */
 //%initial-action {}
 //%destructor { } <>

%union {
    char* str;
}

/* Common tokens */
%token USER
%token CATALOG
%token SCHEMA
%token TABLE
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
        send(session->client_fd, response, strlen(response), 0);
        flog("Status Empty");
        flog_flush();
    }
    |
    header body REQUEST_END {
        /* @todo: here we should reset header or request memory context
            but at the moment we don't have it, we should make it */
        const char *response = "Status: Ready\n\n";
        send(session->client_fd, response, strlen(response), 0);
        flog("Status Ready");
        flog_flush();
    }
    ;

header:
    header_exprs HEADER_END {
        if (session->user == NULL)
            ferr("Parameter 'user' not found in request header");
    }
    ;

header_exprs:
    header_expr
    |
    header_exprs header_expr
    ;

header_expr:
    USER ':' STRING '\n' { session->user = sdup($3); }
    |
    CATALOG ':' STRING '\n' { session->catalog = sdup($3); }
    ;

body:
    cmd ';' { y2parse(session, &cmd->bc, cmd->current); cmd_reset(cmd); }
    |
    body cmd ';' { y2parse(session, &cmd->bc, cmd->current); cmd_reset(cmd); }
    ;

cmd:
    CREATE USER STRING {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_CREATE }));
        bc_put(&cmd->bc, ((BcNode){ .token = BC_USER }));
        bc_put(&cmd->bc, ((BcNode){ .token = BC_STRING, .value.str = $3 }));
    }
    |
    CREATE CATALOG STRING {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_CREATE }));
        bc_put(&cmd->bc, ((BcNode){ .token = BC_CATALOG }));
        bc_put(&cmd->bc, ((BcNode){ .token = BC_STRING, .value.str = $3 }));
    }
    |
    CREATE SCHEMA STRING {
        if (session->catalog == NULL)
            ferr("Parameter 'catalog' not found in request header");
        else {
            bc_put(&cmd->bc, ((BcNode){ .token = BC_CREATE }));
            bc_put(&cmd->bc, ((BcNode){ .token = BC_SCHEMA }));
            bc_put(&cmd->bc, ((BcNode){ .token = BC_STRING, .value.str = $3 }));
        }
    }
    |
    CREATE TABLE STRING '.' STRING {
        if (session->catalog == NULL)
            ferr("Parameter 'catalog' not found in request header");
        else {
            bc_put(&cmd->bc, ((BcNode){ .token = BC_CREATE }));
            bc_put(&cmd->bc, ((BcNode){ .token = BC_TABLE }));
            bc_put(&cmd->bc, ((BcNode){ .token = BC_STRING, .value.str = $3 }));
            bc_put(&cmd->bc, ((BcNode){ .token = BC_STRING, .value.str = $5 }));
            bc_put(&cmd->bc, ((BcNode){ .token = BC_ARRAY_BEGIN }));
        }
    }'(' decls ')' {
        if (session->catalog == NULL)
            ferr("Parameter 'catalog' not found in request header");
        else
            bc_put(&cmd->bc, ((BcNode){ .token = BC_ARRAY_END }));
    }
    |
    INSERT INTO STRING '.' STRING {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_INSERT }));
        bc_put(&cmd->bc, ((BcNode){ .token = BC_STRING, .value.str = $3 }));
        bc_put(&cmd->bc, ((BcNode){ .token = BC_STRING, .value.str = $5 }));
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

decls:
    decl
    |
    decls ',' decl
    ;

decl:
    STRING STRING {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_STRING, .value.str = $1 }));
        bc_put(&cmd->bc, ((BcNode){ .token = BC_STRING, .value.str = $2 }));
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
yyerror(yyscan_t scanner, Session *session, Cmd *cmd, char const *s) {
    (void)scanner;
    (void)session;
    (void)cmd;
    ferr("%s\n", s);
}

