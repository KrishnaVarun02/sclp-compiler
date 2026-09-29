#pragma once
#include "ast.hpp"
#include <iosfwd>
#include <stdexcept>
#include <string>
#include <vector>
struct CompileError : std::runtime_error { using std::runtime_error::runtime_error; };
struct Instruction {
  std::string op;
  int dst=-1, a=-1, b=-1;
  int64_t imm=0;
  std::string target;
  std::vector<int> args;
};
struct Function {
  std::string name;
  Type type;
  int slots=0;
  std::vector<int> params;
  std::vector<Instruction> code;
};
// Registers 0 and 1 are scratch values; registers 2..7 are ABI arguments.
struct Transfer {
  std::string op;
  int reg=0, source=0, slot=-1;
  int64_t imm=0;
  std::string target;
};
struct RTLFunction {
  std::string name;
  int slots;
  std::vector<Transfer> code;
};
std::vector<Function> compile(Node* root);
void optimize(std::vector<Function>&);
std::vector<RTLFunction> lower_rtl(const std::vector<Function>&);
void emit_tac(const std::vector<Function>&, std::ostream&);
void emit_rtl(const std::vector<RTLFunction>&, std::ostream&);
void emit_assembly(const std::vector<RTLFunction>&, const std::string& target, std::ostream&);
