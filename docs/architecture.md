# Compiler architecture

```text
.sclp source
    |
Flex scanner (lexer.l) -> tokens with source locations
    |
Bison LALR parser (parser.y) -> AST (ast.hpp)
    |
signature collection + scoped type checking + TAC generation
    |
optional basic-block constant propagation/folding (-O1)
    |
explicit RTL lowering: loads, stores, scratch registers, ABI arguments
    |
AArch64 macOS or x86-64 Linux assembly
    |
host assembler/linker + runtime.c -> native executable
```

## Front end

The scanner handles keywords, integers, identifiers, operators, comments, and lexical errors. Bison builds arena-owned AST nodes; the arena retains ownership until the compiler exits. Every node carries its source line. Parser error cleanup also frees identifier token strings.

The semantic pass first gathers all function signatures. Function bodies are then checked against a stack of lexical symbol tables. Each variable has a type and a stack-slot identity; shadowed names therefore receive separate storage. A declaration is entered as uninitialized while checking its initializer, which prevents self-reference. Expression checking enforces exact types, call arity, and valid operators. Statement checking tracks loop targets and whether a function has a guaranteed return path.

This implementation fuses semantic analysis and TAC generation. Failed compilation discards the partially generated IR. The output file is opened only after parsing, semantic checks, lowering, and emission have completed successfully.

## Three-address code

TAC uses named storage values `v0`, `v1`, and so on, with explicit control-flow labels. There is one slot for each variable and expression temporary. An example before optimization:

```text
v0 = 2
v1 = 3
v2 = v0 + v1
v3 = 4
v4 = v2 * v3
print v4
```

Short circuit operators lower to conditional branches and moves. Loop conditions are emitted at the top of a loop label, so each iteration computes them again. `break`, `continue`, and returns become explicit transfers of control.

`-O1` tracks known constants inside basic blocks and replaces folds with constant instructions. It clears its map at labels, branches, returns, and calls. Addition, subtraction, and multiplication fold using unsigned arithmetic followed by a bit-preserving conversion back to signed representation; this avoids C++ signed-overflow undefined behavior. Division cases that would produce a runtime error are left intact.

## Register-transfer IR

`lower_rtl` materializes a second IR of `RTLFunction` and `Transfer` objects. Both the RTL text dumper and the assembly backends consume this same representation. Backend code never reads TAC instructions.

RTL specifies two scratch registers, `r0` and `r1`, and up to six argument registers, `a0` through `a5`. It explicitly loads inputs from stack slots, performs register operations, and stores results. A binary operation becomes:

```text
r0 <- slot[0]
r1 <- slot[1]
r0 <- + r0, r1
slot[2] <- r0
```

Calls explicitly load argument registers and place the return value in `r0`. All values that survive calls reside in stack slots. Division and remainder lower to checked C runtime calls. `print` lowers to a runtime call as well, keeping the compiler independent of platform-specific variadic `printf` conventions.

## Native backends

| Property | AArch64 macOS | x86-64 Linux |
| --- | --- | --- |
| Scratch registers | `x9`, `x10` | `rax`, `r10` |
| Argument registers | `x0`–`x5` | `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9` |
| Return register | `x0` | `rax` |
| Frame/link preservation | saved `x29`/`x30` | saved `rbp`; hardware call return address |
| Slot base | `sp` + positive offset | `rbp` + negative offset |
| Frame allocation | rounded up to 16 bytes | rounded up to 16 bytes |
| External symbol prefix | `_` | none |

AArch64 saves the frame/link pair, sets `x29`, and allocates aligned local storage. Large constants use `movz`/`movk`; offsets beyond the direct scaled 12-bit field use `x16` as an address index. Every return reaches a shared epilogue that restores `sp`, `x29`, and `x30`.

The x86-64 prologue pushes `rbp`, records `rsp`, and subtracts the aligned frame size. The stack is therefore 16-byte aligned before each call. Its shared epilogue executes `leave` and `ret`. The Linux output marks the stack non-executable with `.note.GNU-stack`.

User functions are named `sclp_fn_<name>` at the assembly level, except `main`. The runtime symbols occupy a separate namespace. The compiler does not allocate additional callee-saved registers, so there is no hidden register-save set. Native tests execute recursive calls and multi-argument calls to verify that call frames and register clobbers are handled correctly.

## Files

- `src/lexer.l`, `src/parser.y`: actual Flex/Bison sources.
- `src/ast.hpp`: owned AST node interface.
- `src/compiler.hpp`, `src/compiler.cpp`: semantic analysis, TAC, optimization, and RTL lowering.
- `src/backend.cpp`: RTL-to-assembly emission.
- `src/main.cpp`: compiler command line and diagnostics.
- `src/runtime.c`: printing and checked signed division/remainder.
- `tests/run_tests.py`: native integration tests, rejection tests, IR checks, and cross-assembly checks.

## Primary references

The generator interfaces follow the [GNU Bison manual](https://www.gnu.org/software/bison/manual/) and the [Flex manual](https://westes.github.io/flex/manual/). The AArch64 calling convention and stack alignment follow Apple's [Writing ARM64 code for Apple platforms](https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms). The Linux register sequence and stack convention follow the [x86-64 psABI source](https://gitlab.com/x86-psABIs/x86-64-ABI/-/blob/master/x86-64-ABI/low-level-sys-info.tex).
