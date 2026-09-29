#include "compiler.hpp"
#include <cstdint>
#include <ostream>
#include <unordered_map>
namespace {
class Backend {
  std::ostream& out;
  bool arm,mac;
  std::string fname;
  std::string symbol(const std::string& name) const { return (mac?"_":"")+name; }
  std::string function(const std::string& name) const { return symbol(name=="main"?name:"sclp_fn_"+name); }
  std::string label(const std::string& name) const { return ".L"+fname+"_"+name; }
  void line(const std::string& s) { out<<"  "<<s<<"\n"; }
  void constant(int64_t value,const std::string& reg) {
    if(!arm) { line("movabsq $"+std::to_string(value)+", "+reg); return; }
    const uint64_t bits=static_cast<uint64_t>(value);
    line("movz "+reg+", #"+std::to_string(bits&65535));
    for(unsigned shift=16;shift<64;shift+=16) if((bits>>shift)&65535) line("movk "+reg+", #"+std::to_string((bits>>shift)&65535)+", lsl #"+std::to_string(shift));
  }
  std::string address(int slot) const {
    return arm?"[sp, #"+std::to_string(slot*8)+"]":std::to_string(-8*(slot+1))+"(%rbp)";
  }
  void transfer(int slot,const std::string& reg,bool store) {
    if(arm && slot>4095) {
      constant(int64_t(slot)*8,"x16");
      line(std::string(store?"str ":"ldr ")+reg+", [sp, x16]");
    } else if(arm) line(std::string(store?"str ":"ldr ")+reg+", "+address(slot));
    else line("movq "+(store?reg+", "+address(slot):address(slot)+", "+reg));
  }
  std::string arg(size_t n) const {
    static const char* regs[]={"%rdi","%rsi","%rdx","%rcx","%r8","%r9"};
    return arm?"x"+std::to_string(n):regs[n];
  }
  std::string reg(int r) const {
    if(r>=2) return arg(size_t(r-2));
    return arm?(r==0?"x9":"x10"):(r==0?"%rax":"%r10");
  }
  void instruction(const Transfer& i) {
    if(i.op=="label") { out<<label(i.target)<<":\n"; return; }
    if(i.op=="load" || i.op=="store") { transfer(i.slot,reg(i.reg),i.op=="store"); return; }
    if(i.op=="imm") constant(i.imm,reg(i.reg));
    else if(i.op=="mov") line(arm?"mov "+reg(i.reg)+", "+reg(i.source):"movq "+reg(i.source)+", "+reg(i.reg));
    else if(i.op=="call") {
      line(std::string(arm?"bl ":"call ")+symbol(i.target));
      if(arm) line("mov x9, x0");
    } else if(i.op=="return") {
      if(arm) line("mov x0, x9");
      line(std::string(arm?"b ":"jmp ")+label("return"));
    } else if(i.op=="jump") line(std::string(arm?"b ":"jmp ")+label(i.target));
    else if(i.op=="jz" || i.op=="jnz") {
      if(arm) line(std::string(i.op=="jz"?"cbz ":"cbnz ")+reg(0)+", "+label(i.target));
      else { line("testq %rax, %rax"); line(std::string(i.op=="jz"?"jz ":"jnz ")+label(i.target)); }
    } else if(i.op=="neg") line(arm?"neg x9, x9":"negq %rax");
    else if(i.op=="not") {
      if(arm) { line("cmp x9, #0"); line("cset x9, eq"); }
      else { line("testq %rax, %rax"); line("sete %al"); line("movzbq %al, %rax"); }
    } else if(i.op=="+" || i.op=="-" || i.op=="*") {
      std::string operation=i.op=="+"?"add":i.op=="-"?"sub":"mul";
      if(arm) line(operation+" x9, x9, x10");
      else line((i.op=="*"?"imul":operation)+"q %r10, %rax");
    } else {
      static const std::unordered_map<std::string,std::string> cc={{"==","eq"},{"!=","ne"},{"<","lt"},{">","gt"},{"<=","le"},{">=","ge"}};
      auto found=cc.find(i.op); if(found==cc.end()) throw CompileError("internal error: unknown RTL instruction "+i.op);
      if(arm) { line("cmp x9, x10"); line("cset x9, "+found->second); }
      else {
        static const std::unordered_map<std::string,std::string> xcc={{"eq","e"},{"ne","ne"},{"lt","l"},{"gt","g"},{"le","le"},{"ge","ge"}};
        line("cmpq %r10, %rax"); line("set"+xcc.at(found->second)+" %al"); line("movzbq %al, %rax");
      }
    }
  }
public:
  Backend(std::ostream& stream,const std::string& target):out(stream),arm(target=="arm64-macos"),mac(arm) {}
  void emit(const std::vector<RTLFunction>& functions) {
    out<<".text\n";
    for(const auto& f:functions) {
      fname=f.name;
      out<<"\n.globl "<<function(f.name)<<"\n.p2align "<<(arm?2:4)<<"\n"<<function(f.name)<<":\n";
      const int64_t frame=(int64_t(f.slots)*8+15)/16*16;
      if(arm) {
        line("stp x29, x30, [sp, #-16]!"); line("mov x29, sp");
        if(frame) { constant(frame,"x16"); line("sub sp, sp, x16"); }
      } else { line("pushq %rbp"); line("movq %rsp, %rbp"); if(frame) line("subq $"+std::to_string(frame)+", %rsp"); }
      for(const auto& i:f.code) instruction(i);
      out<<label("return")<<":\n";
      if(arm) { line("mov sp, x29"); line("ldp x29, x30, [sp], #16"); line("ret"); }
      else { line("leave"); line("ret"); }
    }
    if(!mac) out<<"\n.section .note.GNU-stack,\"\",@progbits\n";
  }
};
}
void emit_assembly(const std::vector<RTLFunction>& f,const std::string& target,std::ostream& out) {
  if(target!="arm64-macos" && target!="x86_64-linux") throw CompileError("unsupported target '"+target+"'");
  Backend(out,target).emit(f);
}
