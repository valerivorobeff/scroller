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

/* header string */
%x string

/* query */
%s q

%%

{SPACE}+                /* eat */

<INITIAL,string>"$$"    BEGIN(q); return HEADER_END;

<INITIAL>{
    "user"              return USER;
    ":"                 BEGIN(string); return ':';
    "$"                 yylval->str = NULL; return '\n';
}

<string>{
    "$"                 BEGIN(INITIAL); yylval->str = NULL; return '\n';
    {VALUE}             yylval->str = trim(sdup(yytext, yyleng), yyleng); return STRING;
    \n                  /* eat */
    .                   /* eat */
}

<q>{
    "insert"            return INSERT;
    "into"              return INTO;
    "values"            return VALUES;
    ;                   return ';';
    @                   BEGIN(INITIAL); return REQUEST_END;
}

<<EOF>>                 return 0;
\n                      /* eat */
.                       /* eat */

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

