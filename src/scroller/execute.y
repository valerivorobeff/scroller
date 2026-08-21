%code requires{
typedef struct Bc Bc;
typedef struct Session Session;
#include "type.h"
#include <stddef.h>
#include <stdint.h>
}

%code {
#include "array.h"
#include "../../../../src/scroller/bc.h"
#include "../../../../src/scroller/ddl.h"
#include "../../../../src/scroller/dml.h"
#include "../../../../src/scroller/flog.h"
void yyerror(Session *session, Bc *bc, void *current, char const *s);
}

%define api.pure full
%define api.prefix {y2}
%define api.token.prefix {BC_}
 //%locations
%lex-param      {Bc *bc}
%parse-param    {Session *session}
%parse-param    {Bc *bc}
%parse-param    {void *current}

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
}

%token CREATE USER CATALOG SCHEMA TABLE
%token INSERT
%token ARRAY_BEGIN ARRAY_END
%token <integer> INTEGER
%token <str> STRING
%token <type> TYPE
%token <size> SIZE_T

%type <strs> strings
%type <datum> value values

%%

cmd:
    CREATE USER STRING {
        create_user($3);
    }
    |
    CREATE CATALOG STRING {
        create_catalog($3);
        }
    |
    CREATE SCHEMA STRING {
        create_schema(session, $3);
    }
    |
    CREATE TABLE STRING STRING ARRAY_BEGIN decls ARRAY_END {
        create_table(session, $3, $4, (Decl *)current);
    }
    |
    INSERT STRING STRING ARRAY_BEGIN strings ARRAY_END { current = NULL; } ARRAY_BEGIN values ARRAY_END {
        insert(session, $2, $3, (const char **)$5, $9);
    }
    ;

decls:
    decl
    |
    decls decl
    ;

decl:
    SIZE_T TYPE STRING {
        Decl *decl = current;
        array_put(decl, ((Decl){ .name = $3, .size = $1, .type = $2 }));
        current = decl;
    }
    ;

strings:
    STRING {
        char **str = current;
        array_put(str, $1);
        current = str;
        $$ = str;
        flog("y2: %s", $1);
    }
    |
    strings STRING {
        char **str = current;
        array_put(str, $2);
        current = str;
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
        Datum *d = current;
        array_put(d, make_integer($1));
        current = d;
        $$ = d;
    }
    |
    STRING {
        Datum *d = current;
        array_put(d, make_char($1));
        current = d;
        $$ = d;
    }
    ;

%%

/* Called by yyparse on error. */
void
yyerror(Session *session, Bc *bc, void *current, char const *s) {
    (void)session;
    (void)bc;
    (void)current;
    ferr("y2 parser error: %s\n", s);
}

