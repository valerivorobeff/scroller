%{

%}

%option noyywrap
%option never-interactive
%option yylineno
 //%option bison-bridge
 //%option bison-locations
%option reentrant
%option prefix="YY"
%option read
%option full
%option nodefault
%option warn

DIGIT   [0-9]
CHAR    [a-zA-Z_]
PUNKT   ({DIGIT}|{CHAR})
ID      {CHAR}{PUNKT}*
SPACE   [ \t\r]

%%

{SPACE}+    { puts("space"); }
{ID}        { puts("id"); }
":"         puts(":");
"\n"        { puts("enter"); }

%%
/*
int
main() {
    yyscan_t scanner;

    yylex_init ( &scanner );
    yylex ( scanner );
    yylex_destroy ( scanner );

    return 0;
}
*/

