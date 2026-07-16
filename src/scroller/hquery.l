%{
#include "hquery.y.h"
#include "memory.h"
#include <stdio.h>
static char *sdup(const char *src, size_t len);
static char *trim(char *src, size_t len);
%}

%option noyywrap
%option never-interactive
%option yylineno
%option bison-bridge
 //%option bison-locations
%option reentrant
%option prefix="yy"
%option full
%option nodefault
%option warn

ALNUM   [0-9a-zA-Z_]
SPACE   [ \t\r]
VALUE   ({ALNUM}|{SPACE})+

%x string

%%

{SPACE}+        /* eat */
"user"          return USER;
":"             BEGIN(string); return ':';
"$"             return '\n';
<<EOF>>         return 0;
\n              /* eat */
.               /* eat */
<string>"$"     BEGIN(INITIAL); yylval->str = ""; return '\n';
<string>{VALUE} BEGIN(INITIAL); yylval->str = trim(sdup(yytext, yyleng), yyleng); return STRING;
<string>\n      /* eat */
<string>.       /* eat */

%%

char *
sdup(const char *src, size_t len) {
    char *ret = salloc(len + 1);
    memcpy(ret, src, len);
    ret[len] = '\0';

    return ret;
}

char *
trim(char *src, size_t len) {
    char *c;
    if (src == NULL || *src == '\0' || len == 0)
        return src;

    c = src + len - 1;

    while (*src && strchr(" \t\r", *src))
        ++src;

    while (*c && strchr(" \t\r", *c))
        --c;

    *(c + 1) = '\0';

    return src;
}

