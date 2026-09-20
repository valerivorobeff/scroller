%code requires{
typedef struct Session Session;
typedef struct Query Query;
typedef struct Cmd Cmd;
#include "../../../../src/scroller/bc.h"
#include <stdint.h>
}

%code {
#include "hquery.y.h"
#include "hquery.l.h"
#include "array.h"
#include "memory.h"
#include "type.h"
#include "../../../../src/scroller/session.h"
#include "../../../../src/scroller/query.h"
#include "../../../../src/scroller/cmd.h"
#include "../../../../src/scroller/flog.h"
#include <sys/socket.h>

void yyerror(YYLTYPE *location, yyscan_t scanner, Session *session, Query *query, Cmd *cmd, char const *s);
}

%define api.pure full
%define api.prefix {y1}
%locations
%lex-param      {yyscan_t scanner}
%parse-param    {void *scanner}
%parse-param    {Session *session}
%parse-param    {Query *query}
%parse-param    {Cmd *cmd}

/* @todo: write the functions */
 //%initial-action {}
 //%destructor { } <>

%union {
    char* str;
    int64_t integer;
}

/* Common tokens */
%token USER
%token CATALOG
%token <str> STRING

/* Body tokens */
%token <str> ID
%token CREATE
%token SCHEMA TABLE
%token INSERT INTO VALUES
%token SELECT FROM
%token SMALLINT INTEGER BIGINT CHARACTER CHAR VARCHAR VARYING
%token <integer>VINTEGER

%%

session:
    header queries
    ;

header:
    header_exprs '\n' {
        if (session->user == NULL) {
            ferr("Parameter 'user' not found in query header");
            session_send_status(session, SCRS_NO_USER);
        } else
            session_send_status(session, SCRS_OK);
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

queries:
   query
    |
    queries query
    ;

query:
    '\n' {
        /* @todo: here we should reset header or query memory context
            but at the moment we don't have it, we should make it */
        flog("empty query received");
        session_send_status(session, SCRS_OK);
    }
    |
    body '\n' {
        /* @todo: here we should reset header or query memory context
            but at the moment we don't have it, we should make it */
        /* @todo: log query body */
        flog("query received");
        session_send_status(session, SCRS_OK);
    }
    ;

body:
    cmd ';' { y2parse(session, &cmd->bc, cmd->current); cmd_reset(cmd); }
    |
    body cmd ';' { y2parse(session, &cmd->bc, cmd->current); cmd_reset(cmd); }
    ;

cmd:
    CREATE USER ID {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_CREATE }));
        bc_put(&cmd->bc, ((BcNode){ .token = BC_USER }));
        bc_put(&cmd->bc, ((BcNode){ .token = BC_STRING, .value.str = $3 }));
    }
    |
    CREATE CATALOG ID {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_CREATE }));
        bc_put(&cmd->bc, ((BcNode){ .token = BC_CATALOG }));
        bc_put(&cmd->bc, ((BcNode){ .token = BC_STRING, .value.str = $3 }));
    }
    |
    CREATE SCHEMA ID {
        if (session->catalog == NULL)
            ferr("Parameter 'catalog' not found in query header");
        else {
            bc_put(&cmd->bc, ((BcNode){ .token = BC_CREATE }));
            bc_put(&cmd->bc, ((BcNode){ .token = BC_SCHEMA }));
            bc_put(&cmd->bc, ((BcNode){ .token = BC_STRING, .value.str = $3 }));
        }
    }
    |
    CREATE TABLE ID '.' ID {
        if (session->catalog == NULL)
            ferr("Parameter 'catalog' not found in query header");
        else {
            bc_put(&cmd->bc, ((BcNode){ .token = BC_CREATE }));
            bc_put(&cmd->bc, ((BcNode){ .token = BC_TABLE }));
            bc_put(&cmd->bc, ((BcNode){ .token = BC_STRING, .value.str = $3 }));
            bc_put(&cmd->bc, ((BcNode){ .token = BC_STRING, .value.str = $5 }));
            bc_put(&cmd->bc, ((BcNode){ .token = BC_ARRAY_BEGIN }));
        }
    }'(' decls ')' {
        if (session->catalog == NULL)
            ferr("Parameter 'catalog' not found in query header");
        else
            bc_put(&cmd->bc, ((BcNode){ .token = BC_ARRAY_END }));
    }
    |
    INSERT INTO ID '.' ID {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_INSERT }));
        bc_put(&cmd->bc, ((BcNode){ .token = BC_STRING, .value.str = $3 }));
        bc_put(&cmd->bc, ((BcNode){ .token = BC_STRING, .value.str = $5 }));
        bc_put(&cmd->bc, ((BcNode){ .token = BC_ARRAY_BEGIN }));
    } '(' ids ')' {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_ARRAY_END }));
    } VALUES {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_ARRAY_BEGIN }));
    } '(' values ')' {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_ARRAY_END }));
        flog("INSERT INTO %s", $3);
        flog_flush();
    }
    |
    SELECT {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_SELECT }));
        bc_put(&cmd->bc, ((BcNode){ .token = BC_ARRAY_BEGIN }));
    } ids {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_ARRAY_END }));
    } FROM ID '.' ID {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_STRING, .value.str = $6 }));
        bc_put(&cmd->bc, ((BcNode){ .token = BC_STRING, .value.str = $8 }));
    }
    ;

decls:
    decl
    |
    decls ',' decl
    ;

decl:
    ID type {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_STRING, .value.str = $1 }));
    }
    ;

type:
    SMALLINT {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_SIZE_T, .value.size = 0 }));
        bc_put(&cmd->bc, ((BcNode){ .token = BC_TYPE, .value.type = T_SMALLINT }));
    }
    |
    INTEGER {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_SIZE_T, .value.size = 0 }));
        bc_put(&cmd->bc, ((BcNode){ .token = BC_TYPE, .value.type = T_INTEGER }));
    }
    |
    BIGINT {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_SIZE_T, .value.size = 0 }));
        bc_put(&cmd->bc, ((BcNode){ .token = BC_TYPE, .value.type = T_BIGINT }));
    }
    |
    character '(' VINTEGER ')' {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_SIZE_T, .value.size = $3 }));
        bc_put(&cmd->bc, ((BcNode){ .token = BC_TYPE, .value.type = T_CHAR }));
    }
    ;

character:
    CHARACTER
    |
    CHAR
    ;

ids:
    ID {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_STRING, .value.str = $1 }));
        flog("%s", $1);
    }
    |
    ids ',' ID {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_STRING, .value.str = $3 }));
        flog("%s", $3);
    }
    ;

values:
    value
    |
    values ',' value
    ;

value:
    VINTEGER {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_INTEGER, .value.integer = $1 }));
        flog("%l", $1);
    }
    |
    STRING {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_STRING, .value.str = $1 }));
        flog("%s", $1);
    }
    ;

%%

/* Called by yyparse on error. */
void
yyerror(YYLTYPE *location, yyscan_t scanner, Session *session, Query *query, Cmd *cmd, char const *s) {
    (void)location;
    (void)scanner;
    (void)session;
    (void)query;
    (void)cmd;
    ferr("%s\n", s);
}

