%code requires{
typedef struct Bc Bc;
}

%code {
#include "../../../../src/scroller/bc.h"
#include "../../../../src/scroller/flog.h"
void yyerror(Bc *bc, char const *s);
}

%define api.pure full
%define api.prefix {y2}
%define api.token.prefix {BC_}
 //%locations
%lex-param      {Bc *bc}
%parse-param    {Bc *bc}

/* @todo: write the functions */
 //%initial-action {}
 //%destructor { } <>

%union {
    char* str;
}

%token INSERT
%token ARRAY_BEGIN ARRAY_END
%token <str> STRING

%type <str> strings

%%

cmd:
    INSERT STRING ARRAY_BEGIN strings ARRAY_END ARRAY_BEGIN strings ARRAY_END
    ;

strings:
    STRING {
        flog("y2: %s", $1);
    }
    |
    strings STRING {
        flog("y2: %s", $2);
    }
    ;

%%

/* Called by yyparse on error. */
void
yyerror(Bc *bc, char const *s) {
    (void)bc;
    ferr("y2 parser error: %s\n", s);
}

