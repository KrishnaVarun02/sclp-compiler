# SCLP Compiler

A working compiler for a small, statically typed C-like language, built with **Flex, Bison, and C++17**. It produces three-address code, lowers that code to an explicit register-transfer IR, and emits native **AArch64 macOS** or **x86-64 Linux** assembly. Generated programs link against a small C runtime and run directly on the host.

This is a contemporary educational reconstruction inspired by a university compiler project, not recovered Spring 2023 source or a claim of work completed then. The original project description used “GCC Based Compiler.” This implementation uses the GNU compiler-tool ecosystem and can be built with GCC, but it is **an independent compiler, not a GCC fork or GCC plugin**. Its RTL is a custom educational IR, not GCC's internal RTL format.

## Build and run

Requirements: Flex, Bison (2.3 or newer), a C++17 compiler, a C compiler, Make, and Python 3 for tests.

On Ubuntu x86-64:

```sh
sudo apt-get update
sudo apt-get install -y build-essential flex bison python3 clang
make
```

On Apple Silicon macOS, the Xcode Command Line Tools provide the compilers, Flex, Bison, and Make. Install them if needed with `xcode-select --install`, then run `make`. Python 3 must also be available.

```sh
make examples
./build/factorial
./build/control_flow
make test
```

Compile your own program:

```sh
./build/sclpc -O1 examples/recursion.sclp -o build/recursion.s
cc build/recursion.s build/runtime.o -o build/recursion
./build/recursion
# 144
# 1
# 0
```

Inspect the intermediate representations:

```sh
./build/sclpc --emit tac examples/arithmetic.sclp
./build/sclpc --emit tac -O1 examples/arithmetic.sclp
./build/sclpc --emit rtl examples/arithmetic.sclp
```

An explicit target selects the assembly dialect:

```sh
./build/sclpc --target x86_64-linux examples/factorial.sclp -o build/factorial-linux.s
./build/sclpc --target arm64-macos examples/factorial.sclp -o build/factorial-macos.s
```

Cross-target output must be linked with the target's runtime object and toolchain. The example `cc` link command is for the native target.

## Language example

```c
int gcd(int a, int b) {
    while (b != 0) {
        int remainder = a % b;
        a = b;
        b = remainder;
    }
    return a;
}

int main() {
    bool show = true;
    if (show && gcd(1071, 462) == 21) {
        print(21);
    }
    return 0;
}
```

Implemented features:

- `int` and `bool`, typed parameters and returns, forward calls, recursion, and up to six arguments.
- Local variables, block scope, shadowing, zero initialization, and assignment statements.
- `if`/`else`, `while`, nested loops, `break`, `continue`, and early returns.
- Signed integer arithmetic, comparisons, strict boolean operations, and short circuit `&&`/`||`.
- `print(value)`, line and block comments, source line diagnostics, and compile-time rejection of invalid programs.
- Basic-block constant propagation and folding with `-O1`; `-O0` is the default.
- Native function prologues/epilogues, 16-byte stack alignment, per-call stack frames, and ABI register argument passing.
- Explicit runtime errors for division/remainder by zero and signed division overflow.

Read the [language specification](docs/language.md) for precise semantics and the [architecture guide](docs/architecture.md) for IR and calling-convention details.

## Validation

`make test` runs 16 complete programs at both `-O0` and `-O1`. Every program is compiled to assembly, assembled, linked, and executed. Tests cover recursive calls, six arguments, nested loops, lexical scopes, short circuit side effects, integer wrapping, runtime errors, and a frame large enough to exceed AArch64's direct stack-offset encoding. One generated test compares 200 printed results against an independent deterministic Python arithmetic oracle.

The suite also rejects 30 invalid programs, checks that semantic failures produce no output file, checks IR optimization and command-line diagnostics, and cross-assembles both target dialects when Clang is installed. The initial Apple Silicon validation passed **261 assertions**. GitHub Actions is configured to run the suite on Ubuntu x86-64 and Apple Silicon macOS; its actual run status is separate from the local test result.

## Scope

This compiler favors readable passes and predictable execution over production optimization. Every variable and temporary has a stack slot. There is no register allocator, SSA conversion, dead-code elimination, global optimization, floating point, pointer/array/string support, structs, globals, preprocessing, prototypes, `for` loop, or external user-defined C FFI. It does not accept arbitrary C programs. Function arguments beyond six and functions that cannot be shown to return a value are rejected. Deep recursion is limited by the host stack.

The implementation and tests are MIT licensed. Flex/Bison generate scanner/parser files during the build; generated files are not committed.
