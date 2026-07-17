%code requires{
#include "../../../../src/scroller/server.h"
}

%code {
#include "hquery.y.h"
#include "hquery.l.h"

#include <sys/socket.h>

int yylex(YYSTYPE *lvalp, yyscan_t scanner);
//, YYLTYPE *llocp);
void yyerror(yyscan_t scanner, Server *server, char const *s);
}

%define api.pure full
%define api.prefix {yy}
 //%locations
%lex-param      {yyscan_t scanner}
%parse-param    {void *scanner}
%parse-param    {Server *server}

/* @todo: write the functions */
%initial-action { }
 //%destructor { } <>


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
        const char *response = "Status: Ready\n\n";
        send(server->client_fd, response, strlen(response), 0);
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

    }
    ;

%%

#include <stdio.h>

/* Called by yyparse on error. */
void
yyerror(yyscan_t scanner, Server *server, char const *s) {
    (void)scanner;
    (void)server;
    fprintf(stderr, "%s\n", s);
}

