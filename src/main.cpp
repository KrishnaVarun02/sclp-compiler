#include "compiler.hpp"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
extern FILE* yyin;
int yyparse();
int yylex_destroy();
void diagnostic(int line,const std::string& message) { std::cerr<<source_name<<":"<<line<<": error: "<<message<<"\n"; }
int main(int argc,char** argv) {
#if defined(__APPLE__) && defined(__aarch64__)
  std::string target="arm64-macos";
#else
  std::string target="x86_64-linux";
#endif
  std::string mode="asm",output;
  bool optimized=false;
  for(int i=1;i<argc;++i) {
    std::string a=argv[i];
    if(a=="--help" || a=="-h") {
      std::cout<<"Usage: sclpc [--emit asm|tac|rtl] [--target arm64-macos|x86_64-linux] [-O0|-O1] [-o FILE] SOURCE\n"; return 0;
    } else if(a=="--emit" || a=="--target" || a=="-o") {
      if(i+1==argc) { std::cerr<<"missing value for "<<a<<"\n"; return 1; }
      std::string value=argv[++i]; if(a=="--emit") mode=value; else if(a=="--target") target=value; else output=value;
    } else if(a=="-O1") optimized=true;
    else if(a=="-O0") optimized=false;
    else if(!a.empty() && a[0]=='-') { std::cerr<<"unknown option: "<<a<<"\n"; return 1; }
    else if(source_name.empty()) source_name=a;
    else { std::cerr<<"exactly one source file is required\n"; return 1; }
  }
  if(source_name.empty()) { std::cerr<<"source file required (see --help)\n"; return 1; }
  if(mode!="asm" && mode!="tac" && mode!="rtl") { std::cerr<<"unknown output format: "<<mode<<"\n"; return 1; }
  yyin=fopen(source_name.c_str(),"r");
  if(!yyin) { std::perror(source_name.c_str()); return 1; }
  int parsed=yyparse(); fclose(yyin); yylex_destroy();
  if(parsed || parse_failed) return 1;
  try {
    auto functions=compile(ast_root); if(optimized) optimize(functions);
    std::ostringstream text;
    if(mode=="tac") emit_tac(functions,text);
    else {
      auto rtl=lower_rtl(functions);
      if(mode=="rtl") emit_rtl(rtl,text); else emit_assembly(rtl,target,text);
    }
    if(output.empty()) { std::cout<<text.str(); if(!std::cout) throw CompileError("failed to write standard output"); }
    else { std::ofstream file(output); if(!file) throw CompileError("cannot open output '"+output+"'"); file<<text.str(); file.close(); if(!file) throw CompileError("failed to write output '"+output+"'"); }
  } catch(const CompileError& e) { std::cerr<<source_name<<":"<<e.what()<<"\n"; return 1; }
  return 0;
}
