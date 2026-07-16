%{

#include "hquery.y.h"
#include "hquery.l.h"

int yylex(YYSTYPE *lvalp, yyscan_t scanner);
//, YYLTYPE *llocp);
void yyerror(yyscan_t scanner, char const *s);
%}

%define api.pure full
%define api.prefix {yy}
 //%locations
%lex-param      {yyscan_t scanner}
%parse-param    {void *scanner}

%union {
    const char* str;
}

%token <str> USER
%token <str> STRING

%%

header:
    %empty
    |
    header_exprs '\n' {
        fprintf(stderr, "WOW\n");
        YYACCEPT;
    }
    ;

header_exprs:
    header_expr
    |
    header_exprs header_expr
    ;

header_expr:
    USER ':' STRING '\n' {
        fprintf(stderr, "user: '%s'\n", $3 ? $3 : "<NULL>");
    }
    ;

%%

#include <stdio.h>

/* Called by yyparse on error. */
void
yyerror(yyscan_t scanner, char const *s) {
    (void)scanner;
    fprintf(stderr, "%s\n", s);
}

