%{
#include "ast.hpp"
#include <cstdlib>
int yylex();
void yyerror(const char*);
%}
%union { Node* node; char* text; int64_t integer; Type type; }
%locations
%token <text> IDENT
%token <integer> INTEGER
%token INT BOOL TRUE FALSE IF ELSE WHILE RETURN PRINT BREAK CONTINUE
%token EQ NE LE GE AND OR INVALID
%type <node> program functions function params param_items parameter block statements statement
%type <node> initializer expression arguments argument_items
%type <type> type
%destructor { free($$); } IDENT
%left OR
%left AND
%left EQ NE
%left '<' '>' LE GE
%left '+' '-'
%left '*' '/' '%'
%right UMINUS '!'
%nonassoc IF_WITHOUT_ELSE
%nonassoc ELSE
%%
program: functions { ast_root = node(Kind::Program, 1, "", {$1}); };
functions: /* empty */ { $$ = node(Kind::List, 1); }
 | functions function { $$ = append($1, $2); };
function: type IDENT '(' params ')' block {
  $$ = named(Kind::Function, @2.first_line, $2, {$4,$6}); $$->type=$1;
};
type: INT { $$=Type::Int; } | BOOL { $$=Type::Bool; };
params: /* empty */ { $$=node(Kind::List,@$.first_line); } | param_items { $$=$1; };
param_items: parameter { $$=append(node(Kind::List,@1.first_line),$1); }
 | param_items ',' parameter { $$=append($1,$3); };
parameter: type IDENT { $$=named(Kind::Parameter,@2.first_line,$2); $$->type=$1; };
block: '{' statements '}' { $$=node(Kind::Block,@1.first_line,"",{$2}); };
statements: /* empty */ { $$=node(Kind::List,@$.first_line); }
 | statements statement { $$=append($1,$2); };
initializer: /* empty */ { $$=nullptr; } | '=' expression { $$=$2; };
statement:
 block { $$=$1; }
 | type IDENT initializer ';' { $$=named(Kind::Declare,@2.first_line,$2,{$3}); $$->type=$1; }
 | IDENT '=' expression ';' { $$=named(Kind::Assign,@1.first_line,$1,{$3}); }
 | RETURN expression ';' { $$=node(Kind::Return,@1.first_line,"",{$2}); }
 | PRINT '(' expression ')' ';' { $$=node(Kind::Print,@1.first_line,"",{$3}); }
 | IF '(' expression ')' statement %prec IF_WITHOUT_ELSE { $$=node(Kind::If,@1.first_line,"",{$3,$5,nullptr}); }
 | IF '(' expression ')' statement ELSE statement { $$=node(Kind::If,@1.first_line,"",{$3,$5,$7}); }
 | WHILE '(' expression ')' statement { $$=node(Kind::While,@1.first_line,"",{$3,$5}); }
 | BREAK ';' { $$=node(Kind::Break,@1.first_line); }
 | CONTINUE ';' { $$=node(Kind::Continue,@1.first_line); }
 | expression ';' { $$=node(Kind::ExprStmt,@1.first_line,"",{$1}); }
 | ';' { $$=node(Kind::Empty,@1.first_line); };
arguments: /* empty */ { $$=node(Kind::List,@$.first_line); } | argument_items { $$=$1; };
argument_items: expression { $$=append(node(Kind::List,@1.first_line),$1); }
 | argument_items ',' expression { $$=append($1,$3); };
expression:
 INTEGER { $$=node(Kind::Integer,@1.first_line); $$->value=$1; }
 | TRUE { $$=node(Kind::Boolean,@1.first_line); $$->value=1; }
 | FALSE { $$=node(Kind::Boolean,@1.first_line); $$->value=0; }
 | IDENT { $$=named(Kind::Variable,@1.first_line,$1); }
 | IDENT '(' arguments ')' { $$=named(Kind::Call,@1.first_line,$1,{$3}); }
 | '(' expression ')' { $$=$2; }
 | '-' expression %prec UMINUS { $$=node(Kind::Unary,@1.first_line,"-",{$2}); }
 | '!' expression { $$=node(Kind::Unary,@1.first_line,"!",{$2}); }
 | expression '+' expression { $$=node(Kind::Binary,@2.first_line,"+",{$1,$3}); }
 | expression '-' expression { $$=node(Kind::Binary,@2.first_line,"-",{$1,$3}); }
 | expression '*' expression { $$=node(Kind::Binary,@2.first_line,"*",{$1,$3}); }
 | expression '/' expression { $$=node(Kind::Binary,@2.first_line,"/",{$1,$3}); }
 | expression '%' expression { $$=node(Kind::Binary,@2.first_line,"%",{$1,$3}); }
 | expression '<' expression { $$=node(Kind::Binary,@2.first_line,"<",{$1,$3}); }
 | expression '>' expression { $$=node(Kind::Binary,@2.first_line,">",{$1,$3}); }
 | expression LE expression { $$=node(Kind::Binary,@2.first_line,"<=",{$1,$3}); }
 | expression GE expression { $$=node(Kind::Binary,@2.first_line,">=",{$1,$3}); }
 | expression EQ expression { $$=node(Kind::Binary,@2.first_line,"==",{$1,$3}); }
 | expression NE expression { $$=node(Kind::Binary,@2.first_line,"!=",{$1,$3}); }
 | expression AND expression { $$=node(Kind::Binary,@2.first_line,"&&",{$1,$3}); }
 | expression OR expression { $$=node(Kind::Binary,@2.first_line,"||",{$1,$3}); };
%%
void yyerror(const char* msg) { diagnostic(yylloc.first_line,msg); parse_failed=true; }
