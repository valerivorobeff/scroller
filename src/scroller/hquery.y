%{

#include "hquery.y.h"
#include "hquery.l.h"

int yylex(YYSTYPE *lvalp, yyscan_t scanner);
//, YYLTYPE *llocp);
void yyerror(yyscan_t scanner, char const *s);
int parse_query(void);
%}

%define api.pure full
%define api.prefix {yy}
 //%locations
%lex-param      {yyscan_t scanner}
%parse-param    {void *scanner}

%union {
    const char* str;
}

%token <str> STRING

%%

program:
    %empty
    | program expr ';'
    ;

expr:
    ':' '@' {
        fprintf(stderr, "Wow\n");
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

