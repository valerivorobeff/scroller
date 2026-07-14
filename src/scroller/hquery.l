%{
#include "hquery.y.h"
#include <stdio.h>
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

DIGIT   [0-9]
CHAR    [a-zA-Z_]
PUNKT   ({DIGIT}|{CHAR})
ID      {CHAR}{PUNKT}*
SPACE   [ \t\r]

%%

{SPACE}+    /* eat */
{ID}        puts(yytext); yylval->str = yytext; return STRING;
":"         puts(":"); return ':';
"@"         puts("@"); return '@';
";"         { return ';'; }
"\n"        /* eat */

%%

