%code requires{
typedef struct Cmd Cmd;
typedef struct Session Session;
#include "type.h"
#include <stddef.h>
#include <stdint.h>
}

%code {
#include "array.h"
#include "../../../../src/scrs/cmd.h"
#include "../../../../src/scrs/bc.h"
#include "../../../../src/scrs/ddl.h"
#include "../../../../src/scrs/dml.h"
#include "../../../../src/scrs/session.h"
#include "../../../../src/scrs/flog.h"

/* @todo I can't send error message inside check_op macro
         as it is sent by y1parser but it sends just
         SCRS_SERVER ERROR and I want to send:
         session_send_status(session, SCRS_DATUM_TYPE_MISMATCH);
         I don't know how to do it
*/
#define check_op(v1, v2) \
    if (!data_comparable(v1, v2)) { \
        ferr("Data missmatch"); \
        session_send_status(session, SCRS_DATUM_TYPE_MISMATCH); \
        YYABORT; \
    }

#define check_arithmetical(v1, v2) \
    if (!data_arithmetical(v1, v2)) { \
        ferr("Data not arithmetical"); \
        session_send_status(session, SCRS_DATUM_TYPE_MISMATCH); \
        YYABORT; \
    }

#define check_lexical(v1, v2) \
    if (!data_lexical(v1, v2)) { \
        ferr("Data not lexical"); \
        session_send_status(session, SCRS_DATUM_TYPE_MISMATCH); \
        YYABORT; \
    }

static void yyerror(Session *session, Cmd *cmd, char const *s);
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
    Datum datum;
    Datum *datuma;
}

%token CREATE USER CATALOG SCHEMA TABLE
%token INSERT DELETE SELECT
%token WHERE
/* @todo I think I could use '(', ')' or '[]', ']' token pairs instead of the below */
%token <integer> LOOP_BEGIN LOOP_END /* Used inside bc only */
%token ARRAY_BEGIN ARRAY_END
%token <integer> INTEGER
%token <str> STRING
%token <datum> DATUM
%token <type> TYPE
%token <size> SIZE_T

%token OR AND '=' NE '<' LE '>' GE '+' '-' '*' '/' '%' '(' ')' NOT LIKE IN BETWEEN NOT_LIKE NOT_IN NOT_BETWEEN CONCAT
%type <strs> strings
%type <datum> value
%type <datuma> values
%type <integer> expr or_expr and_expr not_expr cmp_expr
%type <datum> additive_expr multiplicative_expr atom

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
    |
    DELETE STRING STRING {
        Titor row;
        ScrcStatus res = dml_delete(session, $2, $3, &row);

        if (res == SCRS_OK) {
            /* Response header */
            session_send_header_int(session, "Status", SCRC_OK);
            session_finish_header(session);
            flog("Delete response header sent");

            cmd->titor = row;
            cmd->bc.titor = row;
        } else
            session_send_status(session, res);
            /* @todo Raise error */

    } delete_mb_where {
        session_send_status(session, SCRS_OK);
    }
    |
    SELECT ARRAY_BEGIN strings ARRAY_END STRING STRING {
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

            /* Table header */
            session_send(session, &scrc_cmd, sizeof(scrc_cmd));     /* Table header start */

            scrc_cmd = SCRC_CMD_ROW;

            for (size_t i = 0; ; ++i) {
                Column *c = htable_get_column(row.header, i);

                if (c == NULL)
                    break;

                /* Skip columns beginning with '*' (system columns) */
                if (c->name[0] == '*')
                    continue;

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
            cmd->bc.titor = row;
        } else {
            session_send_status(session, res);
            /* @todo Raise error */
        }
    } select_mb_where {
        ScrcCmd scrc_cmd = SCRC_CMD_END;
        session_send(session, &scrc_cmd, sizeof(scrc_cmd));          /* Table data finish */
        session_flush(session);
    }
    ;

/**
 * maybe WHERE part for DELETE
 */

delete_mb_where:
    %empty {
        Titor row = cmd->titor;

        if (titor_is_valid(row)) {
            ScrcStatus status = dml_delete_row(session, cmd->titor);
            if (status != SCRS_OK) {
                session_send_status(session, status);
                YYABORT;
            }
        }

        titor_next(&row);
        cmd->titor = row;
    }
    |
    WHERE delete_where
    ;

delete_where:
    delete_where_line
    |
    delete_where delete_where_line
    ;

delete_where_line:
    expr {
        Titor row = cmd->titor;

        if (titor_is_valid(row)) {
            if ($1) {
                ScrcStatus status = dml_delete_row(session, cmd->titor);
                if (status != SCRS_OK) {
                    session_send_status(session, status);
                    YYABORT;
                }
            }

            titor_next(&row);
            cmd->titor = row;
        }
    }
    ;

/**
 * maybe WHERE part for SELECT
 */

select_mb_where:
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
    WHERE select_where
    ;

select_where:
    select_where_line
    |
    select_where select_where_line
    ;

select_where_line:
    expr {
        Titor row = cmd->titor;

        if (titor_is_valid(row)) {
            if ($1) {
                const ScrcCmd scrc_cmd = SCRC_CMD_ROW;
                const size_t sz = titor_get_row_size(row);

                session_send(session, &scrc_cmd, sizeof(scrc_cmd)); /* Row start */
                session_send(session, &sz, sizeof(sz));             /* Row size */
                session_send(session, titor_get_row(row), sz);      /* Row */
            }

            titor_next(&row);
            cmd->titor = row;
        }
    }
    ;

expr:
    or_expr
    ;

or_expr:
    and_expr { $$ = $1; }
    | or_expr OR and_expr { $$ = $1 || $3; }
    ;

and_expr:
    not_expr { $$ = $1; }
    | and_expr AND not_expr { $$ = $1 && $3; }
    ;

not_expr:
    cmp_expr { $$ = $1; }
    | NOT not_expr { $$ = !$2; }
    ;

cmp_expr:
    additive_expr { $$ = !datum_zeroed($1); }
    | additive_expr '=' additive_expr { check_op($1, $3); $$ = eq_data($1, $3); }
    | additive_expr NE additive_expr { check_op($1, $3); $$ = ne_data($1, $3); }
    | additive_expr '<' additive_expr { check_op($1, $3); $$ = lt_data($1, $3); }
    | additive_expr LE additive_expr { check_op($1, $3); $$ = le_data($1, $3); }
    | additive_expr '>' additive_expr { check_op($1, $3); $$ = gt_data($1, $3); }
    | additive_expr GE additive_expr { check_op($1, $3); $$ = ge_data($1, $3); }
    | additive_expr LIKE additive_expr { check_lexical($1, $3); $$ = like_data($1, $3); }
    | additive_expr IN '(' { cmd->current = NULL; } values ')' {
        bool found = false;

        for (int i = 0, ie = array_size($5); i != ie; ++i) {
            if (!data_comparable($1, $5[i])) {
                ferr("Data missmatch");
                session_send_status(session, SCRS_DATUM_TYPE_MISMATCH);
                YYABORT;
            }

            if (eq_data($1, $5[i])) {
                found = true;
                break;
            }
        }

        $$ = found;
    }
    | additive_expr BETWEEN additive_expr additive_expr {
        check_arithmetical($1, $3);
        check_arithmetical($1, $4);
        $$ = ge_data($1, $3) && le_data($1, $4);
    }
    | additive_expr NOT_LIKE additive_expr { check_lexical($1, $3); $$ = !like_data($1, $3); }
    | additive_expr NOT_IN '(' { cmd->current = NULL; } values ')' {
        bool found = false;

        for (int i = 0, ie = array_size($5); i != ie; ++i) {
            if (!data_comparable($1, $5[i])) {
                ferr("Data missmatch");
                session_send_status(session, SCRS_DATUM_TYPE_MISMATCH);
                YYABORT;
            }

            if (eq_data($1, $5[i])) {
                found = true;
                break;
            }
        }

        $$ = !found;
    }
    | additive_expr NOT_BETWEEN additive_expr additive_expr {
        check_arithmetical($1, $3);
        check_arithmetical($1, $4);
        $$ = ge_data($1, $3) && le_data($1, $4);
    }
    ;

additive_expr:
    multiplicative_expr { $$ = $1; }
    | additive_expr '+' multiplicative_expr { check_arithmetical($1, $3); $$ = add_data($1, $3); }
    | additive_expr '-' multiplicative_expr { check_arithmetical($1, $3); $$ = sub_data($1, $3); }
    | additive_expr CONCAT multiplicative_expr { check_lexical($1, $3); $$ = cat_data($1, $3); }
    ;

multiplicative_expr:
    atom { $$ = $1; }
    | multiplicative_expr '*' atom { check_arithmetical($1, $3); $$ = mul_data($1, $3); }
    | multiplicative_expr '/' atom { check_arithmetical($1, $3); $$ = div_data($1, $3); }
    | multiplicative_expr '%' atom {
        if (get_type_group($1.type) != TG_INTEGER ||
            get_type_group($3.type) != TG_INTEGER) {
            ferr("Data not integer");
            session_send_status(session, SCRS_DATUM_TYPE_MISMATCH);
            YYABORT;
        }

        $$ = mod_data($1, $3);
    }
    ;

atom:
    value { $$ = $1; }
    | '(' expr ')' { $$ = make_bigint($2); }  /* integer → Datum */
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
    value {
        Datum *d = cmd->current;
        array_put(d, $1);
        cmd->current = d;
        $$ = d;
    }
    |
    values value {
        Datum *d = cmd->current;
        array_put(d, $2);
        cmd->current = d;
        $$ = d;
    }
    ;

value:
    DATUM {
        if ($1.type == T_NAME) {
            Grid *header;

            if (!titor_is_valid(cmd->titor)) {
                ferr("Unexpected ID");
                goto fin;
            }

            header = cmd->titor.header;

            for (size_t i = 0; ; ++i) {
                Column *c = htable_get_column(header, i);
                if (c == NULL)
                    break;

                if (strcmp($1.value.character, c->name) == 0) {
                    $$ = titor_get_datum(cmd->titor, i);
                    goto fin;
                }
            }

            ferr("Unknown ID \"%s\"", $1.value.character);

        fin:
        }
    }
    ;

%%

/* Called by yyparse on error. */
static void
yyerror(Session *session, Cmd *cmd, char const *s) {
    (void)session;
    ferr("y2 parser error: %s, %li, %i, %li\n",
        s,
        cmd->bc.itor,
        cmd->bc.tokens[cmd->bc.itor].token,
        cmd->bc.tokens[cmd->bc.itor].value.integer
    );
}

