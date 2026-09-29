#include "compiler.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <ostream>
#include <unordered_map>
#include <utility>

Node* ast_root=nullptr;
bool parse_failed=false;
std::string source_name;
static std::vector<std::unique_ptr<Node>> arena;
Node* node(Kind k,int line,std::string text,std::initializer_list<Node*> kids) {
  auto n=std::make_unique<Node>(); n->kind=k; n->line=line; n->text=std::move(text); n->children=kids;
  Node* p=n.get(); arena.push_back(std::move(n)); return p;
}
Node* named(Kind k,int line,char* text,std::initializer_list<Node*> kids) {
  Node* n=node(k,line,text,kids); free(text); return n;
}
Node* append(Node* n,Node* child) { n->children.push_back(child); return n; }
static std::string type_name(Type t) { return t==Type::Int?"int":"bool"; }
namespace {
struct Symbol { Type type; int slot; bool initialized; };
struct Signature { Type type; std::vector<Type> params; };
struct Value { Type type; int slot; };
class Compiler {
  std::unordered_map<std::string,Signature> signatures;
  std::vector<std::unordered_map<std::string,Symbol>> scopes;
  std::vector<std::pair<std::string,std::string>> loops;
  Function* current=nullptr;
  int label_id=0;
  [[noreturn]] void fail(Node* n,const std::string& text) { throw CompileError(std::to_string(n->line)+": "+text); }
  int slot() { return current->slots++; }
  std::string label() { return "L"+std::to_string(label_id++); }
  void emit(std::string op,int dst=-1,int a=-1,int b=-1,int64_t imm=0,std::string target={},std::vector<int> args={}) {
    current->code.push_back({std::move(op),dst,a,b,imm,std::move(target),std::move(args)});
  }
  Symbol* lookup(const std::string& name) {
    for(auto i=scopes.rbegin();i!=scopes.rend();++i) { auto p=i->find(name); if(p!=i->end()) return &p->second; }
    return nullptr;
  }
  void expect(Node* n,Type got,Type required) { if(got!=required) fail(n,"expected "+type_name(required)+", got "+type_name(got)); }
  Value expression(Node* n) {
    if(n->kind==Kind::Integer || n->kind==Kind::Boolean) {
      int d=slot(); emit("const",d,-1,-1,n->value); return {n->kind==Kind::Integer?Type::Int:Type::Bool,d};
    }
    if(n->kind==Kind::Variable) {
      Symbol* s=lookup(n->text); if(!s) fail(n,"undeclared variable '"+n->text+"'");
      if(!s->initialized) fail(n,"variable '"+n->text+"' used in its own initializer");
      return {s->type,s->slot};
    }
    if(n->kind==Kind::Call) {
      if(lookup(n->text)) fail(n,"variable '"+n->text+"' is not callable");
      auto f=signatures.find(n->text); if(f==signatures.end()) fail(n,"unknown function '"+n->text+"'");
      const auto& params=f->second.params; const auto& args=n->children[0]->children;
      if(params.size()!=args.size()) fail(n,"function '"+n->text+"' expects "+std::to_string(params.size())+" arguments, got "+std::to_string(args.size()));
      std::vector<int> values;
      for(size_t i=0;i<args.size();++i) { Value v=expression(args[i]); expect(args[i],v.type,params[i]); values.push_back(v.slot); }
      int d=slot(); emit("call",d,-1,-1,0,n->text,values); return {f->second.type,d};
    }
    if(n->kind==Kind::Unary) {
      Value v=expression(n->children[0]); expect(n,v.type,n->text=="!"?Type::Bool:Type::Int);
      int d=slot(); emit(n->text=="!"?"not":"neg",d,v.slot); return {v.type,d};
    }
    Value a=expression(n->children[0]);
    if(n->text=="&&" || n->text=="||") {
      expect(n,a.type,Type::Bool); int d=slot(); emit("move",d,a.slot);
      std::string end=label(); emit(n->text=="&&"?"jz":"jnz",-1,a.slot,-1,0,end);
      Value b=expression(n->children[1]); expect(n,b.type,Type::Bool); emit("move",d,b.slot); emit("label",-1,-1,-1,0,end);
      return {Type::Bool,d};
    }
    Value b=expression(n->children[1]);
    bool equality=n->text=="==" || n->text=="!=";
    bool comparison=equality || n->text=="<" || n->text==">" || n->text=="<=" || n->text==">=";
    if(equality) expect(n,b.type,a.type); else { expect(n,a.type,Type::Int); expect(n,b.type,Type::Int); }
    int d=slot(); emit(n->text,d,a.slot,b.slot); return {comparison?Type::Bool:Type::Int,d};
  }
  bool arm(Node* n) { scopes.emplace_back(); bool result=statement(n); scopes.pop_back(); return result; }
  bool block(Node* n,bool scoped=true) {
    if(scoped) scopes.emplace_back();
    bool returns=false;
    for(Node* s:n->children[0]->children) { bool r=statement(s); returns=returns||r; }
    if(scoped) scopes.pop_back();
    return returns;
  }
  bool statement(Node* n) {
    switch(n->kind) {
      case Kind::Block: return block(n);
      case Kind::Declare: {
        if(scopes.back().count(n->text)) fail(n,"duplicate variable '"+n->text+"'");
        int d=slot(); scopes.back()[n->text]={n->type,d,false};
        if(n->children[0]) { Value v=expression(n->children[0]); expect(n,v.type,n->type); emit("move",d,v.slot); }
        else emit("const",d,-1,-1,0);
        scopes.back()[n->text].initialized=true; return false;
      }
      case Kind::Assign: {
        Symbol* s=lookup(n->text); if(!s) fail(n,"assignment to undeclared variable '"+n->text+"'");
        const auto symbol=*s; Value v=expression(n->children[0]); expect(n,v.type,symbol.type); emit("move",symbol.slot,v.slot); return false;
      }
      case Kind::Print: { Value v=expression(n->children[0]); emit("print",-1,v.slot); return false; }
      case Kind::ExprStmt: expression(n->children[0]); return false;
      case Kind::Return: {
        Value v=expression(n->children[0]); expect(n,v.type,current->type); emit("return",-1,v.slot); return true;
      }
      case Kind::If: {
        Value v=expression(n->children[0]); expect(n,v.type,Type::Bool); std::string otherwise=label(),end=label();
        emit("jz",-1,v.slot,-1,0,otherwise); bool yes=arm(n->children[1]); emit("jump",-1,-1,-1,0,end);
        emit("label",-1,-1,-1,0,otherwise); bool no=n->children[2] && arm(n->children[2]); emit("label",-1,-1,-1,0,end);
        return yes && no;
      }
      case Kind::While: {
        std::string start=label(),end=label(); emit("label",-1,-1,-1,0,start);
        Value v=expression(n->children[0]); expect(n,v.type,Type::Bool); emit("jz",-1,v.slot,-1,0,end);
        loops.push_back({start,end}); arm(n->children[1]); loops.pop_back();
        emit("jump",-1,-1,-1,0,start); emit("label",-1,-1,-1,0,end); return false;
      }
      case Kind::Break: case Kind::Continue:
        if(loops.empty()) fail(n,n->kind==Kind::Break?"break outside loop":"continue outside loop");
        emit("jump",-1,-1,-1,0,n->kind==Kind::Break?loops.back().second:loops.back().first); return false;
      case Kind::Empty: return false;
      default: fail(n,"internal error: invalid statement");
    }
  }
public:
  std::vector<Function> run(Node* root) {
    const auto& funcs=root->children[0]->children;
    for(Node* f:funcs) {
      if(signatures.count(f->text)) fail(f,"duplicate function '"+f->text+"'");
      Signature signature{f->type,{}};
      for(Node* p:f->children[0]->children) signature.params.push_back(p->type);
      if(signature.params.size()>6) fail(f,"functions support at most 6 parameters");
      signatures[f->text]=std::move(signature);
    }
    auto main=signatures.find("main");
    if(main==signatures.end()) fail(root,"missing int main() entry point");
    if(main->second.type!=Type::Int || !main->second.params.empty()) fail(root,"entry point must be int main()");
    std::vector<Function> output;
    for(Node* f:funcs) {
      output.push_back({f->text,f->type,0,{}, {}}); current=&output.back(); scopes.clear(); scopes.emplace_back();
      for(Node* p:f->children[0]->children) {
        if(scopes.back().count(p->text)) fail(p,"duplicate parameter '"+p->text+"'");
        int d=slot(); scopes.back()[p->text]={p->type,d,true}; current->params.push_back(d);
      }
      if(!block(f->children[1],false)) fail(f,"function '"+f->text+"' may reach its end without returning a value");
    }
    return output;
  }
};
// Convert a bit pattern without implementation-defined signed narrowing.
int64_t wrap(uint64_t bits) { int64_t result; std::memcpy(&result,&bits,sizeof(result)); return result; }
bool fold(const std::string& op,int64_t a,int64_t b,int64_t& out) {
  if(op=="+") out=wrap(uint64_t(a)+uint64_t(b));
  else if(op=="-") out=wrap(uint64_t(a)-uint64_t(b));
  else if(op=="*") out=wrap(uint64_t(a)*uint64_t(b));
  else if(op=="neg") out=wrap(0-uint64_t(a));
  else if(op=="not") out=!a;
  else if(op=="==") out=a==b; else if(op=="!=") out=a!=b;
  else if(op=="<") out=a<b; else if(op==">") out=a>b;
  else if(op=="<=") out=a<=b; else if(op==">=") out=a>=b;
  else if(op=="/" || op=="%") {
    if(b==0 || (a==std::numeric_limits<int64_t>::min() && b==-1)) return false;
    out=op=="/"?a/b:a%b;
  } else return false;
  return true;
}
}
std::vector<Function> compile(Node* root) { return Compiler{}.run(root); }
void optimize(std::vector<Function>& functions) {
  for(auto& f:functions) {
    std::unordered_map<int,int64_t> known;
    for(auto& i:f.code) {
      if(i.op=="label" || i.op=="jump" || i.op=="jz" || i.op=="jnz" || i.op=="return" || i.op=="call") known.clear();
      if(i.op=="move" && known.count(i.a)) { i.imm=known.at(i.a); i.op="const"; i.a=-1; }
      if(i.dst>=0 && i.a>=0 && known.count(i.a) && (i.b<0 || known.count(i.b))) {
        int64_t value; if(fold(i.op,known.at(i.a),i.b<0?0:known.at(i.b),value)) { i.op="const"; i.imm=value; i.a=i.b=-1; }
      }
      if(i.dst>=0) { if(i.op=="const") known[i.dst]=i.imm; else known.erase(i.dst); }
    }
  }
}
void emit_tac(const std::vector<Function>& functions,std::ostream& out) {
  for(const auto& f:functions) {
    out<<"function "<<type_name(f.type)<<" "<<f.name<<"(";
    for(size_t p=0;p<f.params.size();++p) out<<(p?", ":"")<<"v"<<f.params[p];
    out<<") ["<<f.slots<<" slots]\n";
    for(const auto& i:f.code) {
      if(i.op=="label") { out<<i.target<<":\n"; continue; }
      out<<"  ";
      if(i.dst>=0) out<<"v"<<i.dst<<" = ";
      if(i.op=="const") out<<i.imm;
      else if(i.op=="move") out<<"v"<<i.a;
      else if(i.op=="call") { out<<"call "<<i.target<<"("; for(size_t a=0;a<i.args.size();++a) out<<(a?", ":"")<<"v"<<i.args[a]; out<<")"; }
      else if(i.op=="jump") out<<"goto "<<i.target;
      else if(i.op=="jz" || i.op=="jnz") out<<"if v"<<i.a<<(i.op=="jz"?" == 0":" != 0")<<" goto "<<i.target;
      else if(i.b>=0) out<<"v"<<i.a<<" "<<i.op<<" v"<<i.b;
      else out<<i.op<<" v"<<i.a;
      out<<"\n";
    }
    out<<"\n";
  }
}
// This concrete second IR is shared by the readable RTL dump and both backends.
std::vector<RTLFunction> lower_rtl(const std::vector<Function>& functions) {
  std::vector<RTLFunction> result;
  for(const auto& f:functions) {
    RTLFunction rtl{f.name,f.slots,{}};
    auto emit=[&](std::string op,int reg=0,int source=0,int slot=-1,int64_t imm=0,std::string target={}) {
      rtl.code.push_back({std::move(op),reg,source,slot,imm,std::move(target)});
    };
    for(size_t p=0;p<f.params.size();++p) emit("store",int(p)+2,0,f.params[p]);
    for(const auto& i:f.code) {
      if(i.a>=0) emit("load",0,0,i.a);
      if(i.b>=0) emit("load",1,0,i.b);
      if(i.op=="const") emit("imm",0,0,-1,i.imm);
      else if(i.op=="call") {
        for(size_t a=0;a<i.args.size();++a) emit("load",int(a)+2,0,i.args[a]);
        emit("call",0,0,-1,0,i.target=="main"?"main":"sclp_fn_"+i.target);
      } else if(i.op=="/" || i.op=="%") {
        emit("mov",2,0); emit("mov",3,1);
        emit("call",0,0,-1,0,i.op=="/"?"sclp_div":"sclp_mod");
      } else if(i.op=="print") {
        emit("mov",2,0); emit("call",0,0,-1,0,"sclp_print");
      } else if(i.op!="move") emit(i.op,0,0,-1,0,i.target);
      if(i.dst>=0) emit("store",0,0,i.dst);
    }
    result.push_back(std::move(rtl));
  }
  return result;
}
void emit_rtl(const std::vector<RTLFunction>& functions,std::ostream& out) {
  auto reg=[](int r) { return r<2?"r"+std::to_string(r):"a"+std::to_string(r-2); };
  for(const auto& f:functions) {
    out<<"function "<<f.name<<" frame_slots="<<f.slots<<"\n";
    for(const auto& i:f.code) {
      if(i.op=="label") { out<<i.target<<":\n"; continue; }
      out<<"  ";
      if(i.op=="load") out<<reg(i.reg)<<" <- slot["<<i.slot<<"]";
      else if(i.op=="store") out<<"slot["<<i.slot<<"] <- "<<reg(i.reg);
      else if(i.op=="imm") out<<reg(i.reg)<<" <- "<<i.imm;
      else if(i.op=="mov") out<<reg(i.reg)<<" <- "<<reg(i.source);
      else if(i.op=="call") out<<"r0 <- call "<<i.target;
      else if(i.op=="jump") out<<"pc <- "<<i.target;
      else if(i.op=="jz" || i.op=="jnz") out<<"if r0 "<<(i.op=="jz"?"==":"!=")<<" 0: pc <- "<<i.target;
      else if(i.op=="return") out<<"return r0";
      else out<<"r0 <- "<<i.op<<" r0"<<((i.op=="neg" || i.op=="not")?"":", r1");
      out<<"\n";
    }
    out<<"\n";
  }
}
