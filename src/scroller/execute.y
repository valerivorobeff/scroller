%code requires{
typedef struct Bc Bc;
typedef struct Server Server;
}

%code {
#include "array.h"
#include "../../../../src/scroller/bc.h"
#include "../../../../src/scroller/ddl.h"
#include "../../../../src/scroller/dml.h"
#include "../../../../src/scroller/flog.h"
void yyerror(Server *server, Bc *bc, void *current, char const *s);
}

%define api.pure full
%define api.prefix {y2}
%define api.token.prefix {BC_}
 //%locations
%lex-param      {Bc *bc}
%parse-param    {Server *server}
%parse-param    {Bc *bc}
%parse-param    {void *current}

/* @todo: write the functions */
 //%initial-action {}
 //%destructor { } <>

%union {
    char* str;
    char **strs;
}

%token CREATE USER CATALOG SCHEMA TABLE
%token INSERT
%token ARRAY_BEGIN ARRAY_END
%token <str> STRING

%type <strs> strings

%%

cmd:
    CREATE USER STRING {
        create_user(server, $3);
    }
    |
    CREATE CATALOG STRING {
        create_catalog(server, $3);
    }
    |
    CREATE SCHEMA STRING {
        create_schema(server, $3);
    }
    |
    CREATE TABLE STRING STRING ARRAY_BEGIN decls ARRAY_END {
        create_table(server, $3, $4, (Decl *)current);
    }
    |
    INSERT STRING STRING ARRAY_BEGIN strings ARRAY_END { current = NULL; } ARRAY_BEGIN strings ARRAY_END {
        insert(server, $2, $3, (const char **)$5, (const char **)$9);
    }
    ;

decls:
    decl
    |
    decls decl
    ;

decl:
    STRING STRING {
        Decl *decl = current;
        array_put(decl, ((Decl){ .name = $1, .type = $2 }));
        current = decl;
    }

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

%%

/* Called by yyparse on error. */
void
yyerror(Server *server, Bc *bc, void *current, char const *s) {
    (void)server;
    (void)bc;
    (void)current;
    ferr("y2 parser error: %s\n", s);
}

