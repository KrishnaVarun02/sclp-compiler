#pragma once
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

enum class Type { Int, Bool, Unset };
enum class Kind { Program, List, Function, Parameter, Block, Declare, Assign,
  Return, Print, If, While, Break, Continue, ExprStmt, Empty, Integer, Boolean,
  Variable, Call, Binary, Unary };
struct Node {
  Kind kind;
  std::string text;
  int line;
  Type type = Type::Unset;
  int64_t value = 0;
  std::vector<Node*> children;
};
Node* node(Kind, int line, std::string text = {}, std::initializer_list<Node*> = {});
Node* named(Kind, int line, char* text, std::initializer_list<Node*> = {});
Node* append(Node*, Node*);
extern Node* ast_root;
extern bool parse_failed;
extern std::string source_name;
void diagnostic(int line, const std::string& message);
