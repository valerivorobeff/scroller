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

static int execute_cmd(Session *session, Cmd *cmd);
static void yyerror(YYLTYPE *location, yyscan_t scanner, Session *session, Query *query, Cmd *cmd, char const *s);
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
    Datum datum;
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
%token SELECT FROM WHERE
%token SMALLINT INTEGER BIGINT CHARACTER CHAR VARCHAR VARYING
%token <integer>VINTEGER
%type <datum> value

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
    }
    ;

body:
    cmd ';' {
        if (execute_cmd(session, cmd))
            cmd_reset(cmd);
    }
    |
    body cmd ';' {
        if (execute_cmd(session, cmd))
            cmd_reset(cmd);
    }
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
    } mb_where
    ;

mb_where:
    %empty
    |
    WHERE {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_WHERE }));
        bc_put(&cmd->bc, ((BcNode){ .token = BC_LOOP_BEGIN }));
    } expr {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_LOOP_END }));
        for (size_t i = 0, ie = array_size(cmd->bc.tokens); i != ie; ++i)
            printf("token: %i\n", cmd->bc.tokens[i].token);
    }
    ;

expr:
     value '=' value {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_DATUM, .value.datum = $1 }));
        bc_put(&cmd->bc, ((BcNode){ .token = '=' }));
        bc_put(&cmd->bc, ((BcNode){ .token = BC_DATUM, .value.datum = $3 }));
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
    value {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_DATUM, .value.datum = $1 }));
    }
    |
    values ',' value {
        bc_put(&cmd->bc, ((BcNode){ .token = BC_DATUM, .value.datum = $3 }));
    }
    ;

value:
    VINTEGER {
        $$ = make_bigint($1);
        flog("%l", $1);
    }
    |
    STRING {
        $$ = make_char($1);
        flog("%s", $1);
    }
    |
    ID {
        $$ = make_name($1);
        flog("%s", $1);
    }
    ;

%%

static int
execute_cmd(Session *session, Cmd *cmd) {
    int ret = bc_prepare(&cmd->bc);

    switch (ret) {
        case 0:
            break;

        case 1:
            ferr("Bytecode stack overflow");
            session_send_status(session, SCRS_BYTECODE_STACK_OVERFLOW);
            return 1;

        case 2:
            ferr("Bytecode unbalanced stack");
            session_send_status(session, SCRS_BYTECODE_UNBALANCED_STACK);
            return 2;

        default:
            ferr("Bytecode unknown error");
            session_send_status(session, SCRS_BYTECODE_UNKNOWN_ERROR);
            return 3;

    }

    ret = y2parse(session, cmd);

    switch (ret) {
        case 0:
            break;

        case 1:
            ferr("y2 Parser error");
            session_send_status(session, SCRS_PARSER_ERROR);
            return 4;

        case 2:
            ferr("y2 Parser memory exhaustion");
            session_send_status(session, SCRS_PARSER_MEMORY_EXHAUSTION);
            return 5;

        default:
            ferr("y2 Unknown parser error code: %i", ret);
            session_send_status(session, SCRS_UNKNOWN_PARSER_ERROR);
            return 6;
    }

    cmd_reset(cmd);

    return 0;
}

/* Called by yyparse on error. */
static void
yyerror(YYLTYPE *location, yyscan_t scanner, Session *session, Query *query, Cmd *cmd, char const *s) {
    (void)location;
    (void)scanner;
    (void)session;
    (void)query;
    (void)cmd;
    ferr("y1 parser error: %s\n", s);
}

