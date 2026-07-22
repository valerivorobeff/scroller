%code requires{
typedef struct BcNode BcNode;
typedef struct Cmd Cmd;
}

%code {
#include "hquery.y.h"
#include "hquery.l.h"
#include "array.h"
#include "memory.h"
#include "../../../../src/scroller/cmd.h"
#include "../../../../src/scroller/server.h"
#include "../../../../src/scroller/flog.h"

#include <sys/socket.h>

int yylex(YYSTYPE *lvalp, yyscan_t scanner);
//, YYLTYPE *llocp);
void yyerror(yyscan_t scanner, Cmd *cmd, char const *s);
}

%define api.pure full
%define api.prefix {yy}
 //%locations
%lex-param      {yyscan_t scanner}
%parse-param    {void *scanner}
%parse-param    {Cmd *cmd}

/* @todo: write the functions */
 //%initial-action {}
 //%destructor { } <>


%union {
    char* str;
    char **strs;
    BcNode *bc;
}

%token <str> USER
%token <str> STRING
%token <strs> STRINGS
%token HEADER_END
%token REQUEST_END

%token INSERT INTO VALUES
%type <char **>strings

%%

session:
    request
    |
    request session
    ;

request:
    header REQUEST_END {
        const char *response = "Status: Empty\n\n";
        send(cmd->server->client_fd, response, strlen(response), 0);
        flog("Status Empty");
        flog_flush();
    }
    |
    header body REQUEST_END {
        const char *response = "Status: Ready\n\n";
        send(cmd->server->client_fd, response, strlen(response), 0);
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
    INSERT INTO STRING {
        array_put(cmd->bc, ((BcNode){ .token = INSERT }));
        array_put(cmd->bc, ((BcNode){ .token = STRING, .value.str = $3 }));
        array_put(cmd->bc, ((BcNode){ .token = STRINGS, .value.strs = array_create(*cmd->current, 1024) }));
        cmd->current = &array_back_ref(cmd->bc).value.strs;
    } '(' strings ')' {
        flog("INSERT INTO %s", $3);
        for (size_t i = 0, ie = array_size($<strs>6); i != ie; ++i)
            flog("%lu: %s", i, $<strs>6[i]);
        flog_flush();
    }

strings:
    STRING {
          array_put(*cmd->current, $1);
          $<strs>$ = *cmd->current;
    }
    |
    STRING ',' strings {
          array_put(*cmd->current, $1);
          $<strs>$ = $<strs>3;
    }
    ;

%%

/* Called by yyparse on error. */
void
yyerror(yyscan_t scanner, Cmd *cmd, char const *s) {
    (void)scanner;
    (void)cmd;
    ferr("%s\n", s);
}

