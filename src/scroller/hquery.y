%code requires{
#include "../../../../src/scroller/server.h"
}

%code {
#include "hquery.y.h"
#include "hquery.l.h"
#include "../../../../src/scroller/flog.h"

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
 //%initial-action { }
 //%destructor { } <>


%union {
    const char* str;
}

%token <str> USER
%token <str> STRING
%token HEADER_END
%token REQUEST_END

%token INSERT INTO VALUES

%%

session:
    request
    |
    request session
    ;

request:
    header REQUEST_END {
        const char *response = "Status: Empty\n\n";
        send(server->client_fd, response, strlen(response), 0);
        flog("Status Empty");
        flog_flush();
    }
    |
    header body REQUEST_END {
        const char *response = "Status: Ready\n\n";
        send(server->client_fd, response, strlen(response), 0);
        flog("Status Ready");
        flog_flush();
    }
    ;

header:
    header_exprs HEADER_END
    ;

header_exprs:
    header_expr
    |
    header_expr header_exprs
    ;

header_expr:
    USER ':' STRING '\n'
    ;

body:
    cmd ';'
    |
    body cmd ';'
    ;

cmd:
    INSERT INTO {
        flog("INSERT INTO");
        flog_flush();
    }

%%

/* Called by yyparse on error. */
void
yyerror(yyscan_t scanner, Server *server, char const *s) {
    (void)scanner;
    (void)server;
    ferr("%s\n", s);
}

