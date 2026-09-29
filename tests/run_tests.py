#!/usr/bin/env python3
"""End-to-end tests compile, assemble, link, and execute generated programs."""
import math
import os
from pathlib import Path
import platform
import random
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / "build/sclpc"
RUNTIME = ROOT / "build/runtime.o"
CC = shlex.split(os.environ.get("CC", "cc"))
COUNT = 0

def run(args, **kwargs):
    return subprocess.run([str(x) for x in args], text=True, capture_output=True, timeout=20, **kwargs)

def check(ok, message):
    global COUNT
    if not ok:
        raise AssertionError(message)
    COUNT += 1

def execute(directory, name, source, expected, code=0, error=""):
    src = directory / (name + ".sclp")
    src.write_text(source)
    for optimization in ("-O0", "-O1"):
        asm = directory / (name + optimization + ".s")
        exe = directory / (name + optimization)
        result = run([COMPILER, optimization, src, "-o", asm])
        check(result.returncode == 0, f"{name} compile: {result.stderr}")
        result = run([*CC, asm, RUNTIME, "-o", exe])
        check(result.returncode == 0, f"{name} link: {result.stderr}")
        result = run([exe])
        check(result.returncode == code, f"{name} exit: {result.returncode}; {result.stderr}")
        check(result.stdout == expected, f"{name}: expected {expected!r}, got {result.stdout!r}")
        check(error in result.stderr, f"{name}: expected runtime error {error!r}: {result.stderr}")

def invalid(directory, name, source, message):
    src = directory / (name + ".sclp")
    dst = directory / (name + ".s")
    src.write_text(source)
    result = run([COMPILER, src, "-o", dst])
    check(result.returncode != 0, name + " should be rejected")
    check(message in result.stderr, f"{name}: missing {message!r}: {result.stderr}")
    check(not dst.exists(), name + " created output for invalid source")

def main():
    host = (platform.system(), platform.machine())
    if host not in (("Darwin", "arm64"), ("Linux", "x86_64"), ("Linux", "AMD64")):
        raise SystemExit(f"Native integration tests require Apple Silicon macOS or x86_64 Linux; got {host}")
    with tempfile.TemporaryDirectory(prefix="sclp-tests-") as temp:
        d = Path(temp)
        examples = {
            "factorial": "".join(f"{math.factorial(i)}\n" for i in range(11)),
            "control_flow": "63\n99\n63\n0\n",
            "recursion": "144\n1\n0\n",
            "arithmetic": "21\n-3\n-2\n45\n-9223372036854775808\n",
        }
        for name, expected in examples.items():
            execute(d, name, (ROOT / "examples" / (name + ".sclp")).read_text(), expected)
        execute(d, "six_args", """
int add(int a,int b,int c,int d,int e,int f) { return a+b*2+c*3+d*4+e*5+f*6; }
int twice(int a) { return a*2; }
int main() { print(add(twice(1),twice(2),twice(3),twice(4),twice(5),twice(6))); return 0; }
""", "182\n")
        execute(d, "short_circuit", """
bool probe() { print(123); return true; }
int main() {
 print(false && probe()); print(true || probe());
 print(true && probe()); print(false || probe());
 print(!(false || false)); return 0;
}
""", "0\n1\n123\n1\n123\n1\n1\n")
        execute(d, "nested_loops", """
int main() { int i=0; int sum=0;
 while(i<4) { i=i+1; int j=0;
  while(j<4) { j=j+1; if(j==2) continue; if(i==3) break; sum=sum+i*j; }
 } print(sum); return 0; }
""", "56\n")
        execute(d, "boolean_ops", """
bool eq(bool a,bool b) { return a==b; }
int main() { bool z; int n;
 print(z); print(n); print(eq(true,!false)); print(true != false);
 print(-2<-1); print(-2>-1); print(3>=3); print(3<=3);
 print(1+2*3==7 && false || true); return 0; }
""", "0\n0\n1\n1\n1\n0\n1\n1\n1\n")
        execute(d, "dangling_else", "int main(){if(true) if(false) print(1); else print(2); return 0;}", "2\n")
        execute(d, "exit_status", "int main(){return 7;}", "", code=7)
        execute(d, "comments", "/* header\n comment */ int main() { // line\n print(42); return 0; }", "42\n")
        # Exceeds AArch64's 12-bit scaled stack-offset field and 16-bit frame immediate.
        declarations = "\n".join(f"int v{i}={i};" for i in range(4200))
        execute(d, "large_frame", "int main(){" + declarations + "print(v4199);return 0;}", "4199\n")
        execute(d, "div_zero", "int main(){int n=0;print(1/n);return 0;}", "", code=2, error="division by zero")
        execute(d, "mod_zero", "int main(){print(1%0);return 0;}", "", code=2, error="division by zero")
        execute(d, "div_overflow", "int main(){int n=-9223372036854775807-1;print(n / -1);return 0;}", "", code=2, error="signed division overflow")
        # Independent Python oracle for 50 deterministic arithmetic/comparison cases.
        rng = random.Random(2023)
        statements, expected = [], []
        for _ in range(50):
            a, b, c = (rng.randint(-1000, 1000) for _ in range(3))
            divisor = rng.choice([v for v in range(-20, 21) if v])
            value = (a + b) * c
            quotient = abs(value) // abs(divisor) * (-1 if (value < 0) != (divisor < 0) else 1)
            remainder = value - quotient * divisor
            statements += [f"print(({a}+{b})*{c});", f"print((({a}+{b})*{c})/{divisor});", f"print((({a}+{b})*{c})%{divisor});", f"print({a}<{b});"]
            expected += [value, quotient, remainder, int(a < b)]
        execute(d, "oracle", "int main(){" + "".join(statements) + "return 0;}", "".join(f"{n}\n" for n in expected))
        cases = [
            ("undeclared", "int main(){print(x);return 0;}", "undeclared variable 'x'"),
            ("assignment", "int main(){x=1;return 0;}", "assignment to undeclared"),
            ("duplicate_local", "int main(){int x;int x;return 0;}", "duplicate variable"),
            ("duplicate_param", "int f(int x,int x){return x;} int main(){return 0;}", "duplicate parameter"),
            ("param_redeclare", "int f(int x){int x;return x;} int main(){return 0;}", "duplicate variable"),
            ("duplicate_function", "int main(){return 0;} int main(){return 1;}", "duplicate function"),
            ("arity", "int f(int a){return a;} int main(){return f();}", "expects 1 arguments"),
            ("unknown_function", "int main(){return missing();}", "unknown function"),
            ("call_type", "int f(bool x){return 1;} int main(){return f(1);}", "expected bool, got int"),
            ("shadow_call", "int f(){return 0;} int main(){int f;return f();}", "not callable"),
            ("initializer_type", "int main(){int x=true;return 0;}", "expected int, got bool"),
            ("assignment_type", "int main(){bool x;x=1;return 0;}", "expected bool, got int"),
            ("return_type", "int main(){return true;}", "expected int, got bool"),
            ("condition_type", "int main(){if(1) return 1;return 0;}", "expected bool, got int"),
            ("logical_type", "int main(){print(1&&true);return 0;}", "expected bool, got int"),
            ("arithmetic_type", "int main(){print(true+false);return 0;}", "expected int, got bool"),
            ("equality_type", "int main(){print(1==true);return 0;}", "expected int, got bool"),
            ("scope", "int main(){{int x=1;} print(x);return 0;}", "undeclared variable"),
            ("branch_scope", "int main(){if(true) int x=1;print(x);return 0;}", "undeclared variable"),
            ("self_initializer", "int main(){int x=x;return 0;}", "own initializer"),
            ("break", "int main(){break;return 0;}", "break outside loop"),
            ("continue", "int main(){continue;return 0;}", "continue outside loop"),
            ("missing_return", "int main(){if(true)return 1;}", "without returning"),
            ("missing_main", "int f(){return 1;}", "missing int main"),
            ("main_signature", "bool main(){return true;}", "entry point must be"),
            ("too_many_args", "int f(int a,int b,int c,int d,int e,int f,int g){return 0;}int main(){return 0;}", "at most 6"),
            ("literal_range", "int main(){return 9223372036854775808;}", "signed 64-bit range"),
            ("bad_character", "int main(){return @;}", "unexpected character"),
            ("unterminated_comment", "int main(){return 0;} /*", "unterminated block comment"),
            ("syntax", "int main(){int x=;return 0;}", "syntax error"),
        ]
        for case in cases:
            invalid(d, *case)
        sample = d / "ir.sclp"
        sample.write_text("int main(){print((2+3)*4);return 0;}")
        tac0 = run([COMPILER, "--emit", "tac", sample])
        tac1 = run([COMPILER, "--emit", "tac", "-O1", sample])
        rtl = run([COMPILER, "--emit", "rtl", sample])
        check(tac0.returncode == tac1.returncode == rtl.returncode == 0, "IR output failed")
        check(" * " in tac0.stdout and " * " not in tac1.stdout and "= 20" in tac1.stdout, "constant folding missing")
        check("slot[" in rtl.stdout and "r0 <-" in rtl.stdout and "call sclp_print" in rtl.stdout, "RTL lacks register transfers")
        for option in (["--emit", "bad"], ["--target", "bad"], ["--bad"], ["-o"]):
            result = run([COMPILER, sample, *option])
            check(result.returncode != 0, f"invalid option {option} accepted")
        # Both assembly dialects must be accepted by LLVM's integrated assembler.
        clang = shutil.which("clang")
        if clang:
            for target, triple in (("arm64-macos", "arm64-apple-macos11"), ("x86_64-linux", "x86_64-unknown-linux-gnu")):
                asm = d / (target + ".s")
                result = run([COMPILER, ROOT / "examples/arithmetic.sclp", "--target", target, "-o", asm])
                check(result.returncode == 0, result.stderr)
                result = run([clang, "-target", triple, "-c", asm, "-o", d / (target + ".o")])
                check(result.returncode == 0, f"{target} assembly invalid: {result.stderr}")
        else:
            print("SKIP: cross-target assembly checks (clang not installed)")
    print(f"PASS: {COUNT} assertions; 16 programs at -O0 and -O1, 30 rejected programs, IR and CLI checks")

if __name__ == "__main__":
    main()
