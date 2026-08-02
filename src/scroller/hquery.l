%top{
#define YYSTYPE Y1STYPE
}

%{
#include "hquery.y.h"
#include "memory.h"
#include <stdio.h>
static char *sdupl(const char *src, size_t len);
static char *trim(char *src, size_t len);
%}

%option noyywrap
%option never-interactive
%option yylineno
%option case-insensitive
%option bison-bridge
 //%option bison-locations
%option reentrant
%option prefix="y1"
%option full
%option nodefault
%option noinput
%option nounput
%option warn

NUM             [0-9]
ALPHA           [a-zA-Z_]
ALNUM           ({NUM}|{ALPHA})
SPACE           [ \t\r]
HEADER_VALUE    ({ALNUM}|{SPACE})+
STRING          {ALPHA}{ALNUM}*

/* header string */
%x string

/* query */
%s query

%%

{SPACE}+                /* eat */

<INITIAL,string>"$$"    BEGIN(query); return HEADER_END;

<INITIAL>{
    "user"              return USER;
    "catalog"           return CATALOG;
    ":"                 BEGIN(string); return ':';
    "$"                 yylval->str = NULL; return '\n';
}

<string>{
    "$"                 BEGIN(INITIAL); yylval->str = NULL; return '\n';
    {HEADER_VALUE}      yylval->str = trim(sdupl(yytext, yyleng), yyleng); return STRING;
    \n                  /* eat */
    .                   /* eat */
}

<query>{
    "create"            return CREATE;
    "user"              return USER;
    "catalog"           return CATALOG;
    "schema"            return SCHEMA;
    "table"             return TABLE;
    "insert"            return INSERT;
    "into"              return INTO;
    "values"            return VALUES;
    "("                 return '(';
    ")"                 return ')';
    ","                 return ',';
    "."                 return '.';
    {STRING}            yylval->str = sdupl(yytext, yyleng); return STRING;
    ;                   return ';';
    @                   BEGIN(INITIAL); return REQUEST_END;
}

<<EOF>>                 return 0;
\n                      /* eat */

%%

char *
sdupl(const char *src, size_t len) {
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

