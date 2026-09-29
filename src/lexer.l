%option noyywrap yylineno nounput noinput
%x COMMENT
%{
#include "ast.hpp"
#include "parser.hpp"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#define YY_USER_ACTION yylloc.first_line = yylloc.last_line = yylineno;
static int comment_line;
%}
%%
[ \t\r\n]+          ;
"//"[^\n]*          ;
"/*"                { comment_line=yylineno; BEGIN(COMMENT); }
<COMMENT>"*/"        BEGIN(INITIAL);
<COMMENT>\n          ;
<COMMENT>.           ;
<COMMENT><<EOF>>     { diagnostic(comment_line,"unterminated block comment"); parse_failed=true; return 0; }
"int"               return INT;
"bool"              return BOOL;
"true"              return TRUE;
"false"             return FALSE;
"if"                return IF;
"else"              return ELSE;
"while"             return WHILE;
"return"            return RETURN;
"print"             return PRINT;
"break"             return BREAK;
"continue"          return CONTINUE;
"=="                return EQ;
"!="                return NE;
"<="                return LE;
">="                return GE;
"&&"                return AND;
"||"                return OR;
[0-9]+              { errno=0; yylval.integer=strtoll(yytext,nullptr,10); if(errno==ERANGE) { diagnostic(yylineno,"integer literal exceeds signed 64-bit range"); parse_failed=true; return INVALID; } return INTEGER; }
[a-zA-Z_][a-zA-Z0-9_]* { yylval.text=strdup(yytext); return IDENT; }
[{}(),;=+*/%<>!\-]  return yytext[0];
.                   { diagnostic(yylineno,std::string("unexpected character '")+yytext+"'"); parse_failed=true; return INVALID; }
%%
