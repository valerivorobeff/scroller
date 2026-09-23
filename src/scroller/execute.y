%code requires{
typedef struct Cmd Cmd;
typedef struct Session Session;
#include "type.h"
#include <stddef.h>
#include <stdint.h>
}

%code {
#include "array.h"
#include "../../../../src/scroller/cmd.h"
#include "../../../../src/scroller/bc.h"
#include "../../../../src/scroller/ddl.h"
#include "../../../../src/scroller/dml.h"
#include "../../../../src/scroller/session.h"
#include "../../../../src/scroller/flog.h"
void yyerror(Session *session, Cmd *cmd, char const *s);
}

%define api.pure full
%define api.prefix {y2}
%define api.token.prefix {BC_}
 //%locations
%lex-param      {Cmd *cmd}
%parse-param    {Session *session}
%parse-param    {Cmd *cmd}

/* @todo: write the functions */
 //%initial-action {}
 //%destructor { } <>

%union {
    char* str;
    char **strs;
    int type;
    size_t size;
    int64_t integer;
    Datum *datum;
    /*struct {
        int loop;
        int dest;
    } cmd;*/
}

%token CREATE USER CATALOG SCHEMA TABLE
%token INSERT SELECT
%token WHERE_BEGIN WHERE_END
%token ARRAY_BEGIN ARRAY_END
%token <integer> INTEGER
%token <str> STRING
%token <type> TYPE
%token <size> SIZE_T

%type <strs> strings
%type <datum> value values
%type <integer> expr

%%

cmd:
    CREATE USER STRING {
        session_send_status(session, create_user($3));
    }
    |
    CREATE CATALOG STRING {
        session_send_status(session, create_catalog($3));
    }
    |
    CREATE SCHEMA STRING {
        session_send_status(session, create_schema(session, $3));
    }
    |
    CREATE TABLE STRING STRING ARRAY_BEGIN decls ARRAY_END {
        session_send_status(session, create_table(session, $3, $4, (Decl *)cmd->current));
    }
    |
    INSERT STRING STRING ARRAY_BEGIN strings ARRAY_END { cmd->current = NULL; } ARRAY_BEGIN values ARRAY_END {
        session_send_status(session, insert(session, $2, $3, (const char **)$5, $9));
    }
    | SELECT ARRAY_BEGIN strings ARRAY_END STRING STRING {
        Titor row;
        ScrcStatus res = dml_select(session, $5, $6, (const char **)$3, &row);
        if (res == SCRS_OK) {
            ScrcCmd scrc_cmd = SCRC_CMD_TABHEADER;
            size_t sz;

            /* Response header */
            session_send_header_int(session, "Status", SCRC_OK);
            session_send_header_str(session, "Body", "Yes");
            session_finish_header(session);
            flog("Select response header sent");

            /* Table geader */
            session_send(session, &scrc_cmd, sizeof(scrc_cmd));     /* Table header start */

            scrc_cmd = SCRC_CMD_ROW;

            for (size_t i = 0; ; ++i) {
                Column *c = htable_get_column(row.header, i);
                if (c == NULL)
                    break;

                session_send(session, &scrc_cmd, sizeof(scrc_cmd)); /* Column start */
                sz = sizeof(Column);
                session_send(session, &sz, sizeof(sz));             /* Column size */
                session_send(session, c, sizeof(Column));           /* Column */
            }

            scrc_cmd = SCRC_CMD_END;
            session_send(session, &scrc_cmd, sizeof(scrc_cmd));     /* Table header finish */

            /* Table data */
            scrc_cmd = SCRC_CMD_TABDATA;
            session_send(session, &scrc_cmd, sizeof(scrc_cmd));     /* Table data start */

            cmd->titor = row;
        } else {
            session_send_status(session, res);
            /* @todo Raise error */
        }
    } mb_where {
        ScrcCmd scrc_cmd = SCRC_CMD_END;
        session_send(session, &scrc_cmd, sizeof(scrc_cmd));          /* Table data finish */
        session_flush(session);
    }
    ;

mb_where:
    %empty {
        Titor row = cmd->titor;
        const ScrcCmd scrc_cmd = SCRC_CMD_ROW;

        if (titor_is_valid(row)) {
            const size_t sz = titor_get_row_size(row);

            for (; titor_is_valid(row); titor_next(&row)) {
                session_send(session, &scrc_cmd, sizeof(scrc_cmd)); /* Row start */
                session_send(session, &sz, sizeof(sz));             /* Row size */
                session_send(session, titor_get_row(row), sz);      /* Row */
            }
        }
    }
    |
    where
    ;

where:
    where_line
    |
    where where_line
    ;

where_line:
    WHERE_BEGIN expr WHERE_END {
        Titor row = cmd->titor;
        const ScrcCmd scrc_cmd = SCRC_CMD_ROW;

        if ($2 && titor_is_valid(row)) {
            const size_t sz = titor_get_row_size(row);

            session_send(session, &scrc_cmd, sizeof(scrc_cmd)); /* Row start */
            session_send(session, &sz, sizeof(sz));             /* Row size */
            session_send(session, titor_get_row(row), sz);      /* Row */

            titor_next(&row);
            cmd->titor = row;
        }
    }
    ;

expr:
    INTEGER '=' INTEGER { $$ = $1 == $3; }
    ;

decls:
    decl
    |
    decls decl
    ;

decl:
    SIZE_T TYPE STRING {
        Decl *decl = cmd->current;
        array_put(decl, ((Decl){ .name = $3, .size = $1, .type = $2 }));
        cmd->current = decl;
    }
    ;

strings:
    STRING {
        char **str = cmd->current;
        array_put(str, $1);
        cmd->current = str;
        $$ = str;
        flog("y2: %s", $1);
    }
    |
    strings STRING {
        char **str = cmd->current;
        array_put(str, $2);
        cmd->current = str;
        $$ = str;
        flog("y2: %s", $2);
    }
    ;

values:
    value
    |
    values value
    ;

value:
    INTEGER {
        Datum *d = cmd->current;
        array_put(d, make_bigint($1));
        cmd->current = d;
        $$ = d;
    }
    |
    STRING {
        Datum *d = cmd->current;
        array_put(d, make_char($1));
        cmd->current = d;
        $$ = d;
    }
    ;

%%

/* Called by yyparse on error. */
void
yyerror(Session *session, Cmd *cmd, char const *s) {
    (void)session;
    (void)cmd;
    ferr("y2 parser error: %s\n", s);
}

