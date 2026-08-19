%top{
#define YYSTYPE Y1STYPE
#define YYLTYPE Y1LTYPE
typedef struct Context Context;
}

%{
#include "hquery.y.h"
#include "memory.h"
#include <stdio.h>
static char *sdupl(const char *src, size_t len);
static char *trim(char *src, size_t len);
static char *str_unescape(const char *s, size_t len, char q);
%}

%option noyywrap
%option never-interactive
%option yylineno
%option case-insensitive
%option bison-bridge
%option bison-locations
%option reentrant
%option prefix="y1"
%option extra-type="Context *"
%option noyyalloc
%option noyyrealloc
%option noyyfree
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
INTEGER         -?(0|[1-9]{NUM}*)
STRING          {ALPHA}{ALNUM}*
DQUOTED         \"(\\.|[^\"])*\"
QUOTED          \'(\\.|[^\'])*\'

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
    "smallint"          return SMALLINT;
    "integer"           return INTEGER;
    "int"               return INTEGER;
    "bigint"            return BIGINT;
    "character"         return CHARACTER;
    "char"              return CHAR;
    "varchar"           return VARCHAR;
    "varying"           return VARYING;
    "("                 return '(';
    ")"                 return ')';
    ","                 return ',';
    "."                 return '.';
    {INTEGER}           yylval->integer = atol(yytext); return VINTEGER;
    {STRING}            yylval->str = sdupl(yytext, yyleng); return ID;
    {DQUOTED}           yylval->str = str_unescape(yytext, yyleng, '"'); return ID;
    {QUOTED}            yylval->str = str_unescape(yytext, yyleng, '\''); return STRING;
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

char *
str_unescape(const char *s, size_t len, char q) {
    char *ret = salloc(len + 1);
    char *d = ret;

    for (++s; *s && *s != q; ++d, ++s) {
        if (*s == '\\') {
            ++s;
            if (*s == q)
                *d = q;
            else {
                switch (*s) {
                    case '\n': *d = '\n'; break;
                    case '\t': *d = '\t'; break;
                    case '\\': *d = '\\'; break;
                }
            }
        } else
            *d = *s;
    }

    *d = '\0';

    return ret;
}

inline void *
y1alloc(size_t sz, yyscan_t scanner) {
    return context_alloc(yyget_extra(scanner), sz);
}

inline void *
y1realloc(void *p, size_t sz, yyscan_t scanner) {
    return context_realloc(yyget_extra(scanner), p, sz);
}

inline void
y1free(void *p, yyscan_t scanner) {
    return context_free(yyget_extra(scanner), p);
}

