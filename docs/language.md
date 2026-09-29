# SCLP language specification

A translation unit is a sequence of function definitions. It must contain exactly one `int main()` entry point. All functions are known before their bodies are checked, allowing forward calls and mutual recursion. Function names are unique and functions take at most six parameters.

## Types and values

`int` is a signed 64-bit two's-complement integer. Decimal integer tokens range from `0` to `9223372036854775807`; negative values use unary minus. Write the minimum integer as `-9223372036854775807 - 1`, because the positive token `9223372036854775808` is rejected.

`bool` has values `true` and `false`, represented internally as 1 and 0. There are no implicit conversions. An `int` cannot be used as a condition or passed to a `bool` parameter. `print` accepts either type and writes its numeric representation followed by a newline.

Arithmetic `+`, `-`, `*`, and unary `-` wrap modulo 2^64. Division truncates toward zero, and the remainder has the dividend's sign. Division and remainder by zero terminate the program with status 2 and a diagnostic on standard error. Both operations also reject `INT64_MIN` with divisor `-1`. These rules are preserved by constant folding.

## Expressions

From lowest to highest precedence:

| Operators | Operand types | Result |
| --- | --- | --- |
| `\|\|` | bool, bool | bool |
| `&&` | bool, bool | bool |
| `==`, `!=` | matching types | bool |
| `<`, `>`, `<=`, `>=` | int, int | bool |
| `+`, `-` | int, int | int |
| `*`, `/`, `%` | int, int | int |
| unary `-`, `!` | int for `-`, bool for `!` | operand type |

Binary operators are left-associative. Parentheses override precedence. Operands and function arguments are evaluated left to right. `&&` evaluates its right operand only when its left operand is true; `||` does so only when its left operand is false. Function calls can appear inside expressions. Assignment is a statement, not an expression.

## Statements and scope

```text
statement := { statement* }
           | type name [= expression] ;
           | name = expression ;
           | expression ;
           | return expression ;
           | print ( expression ) ;
           | if ( expression ) statement [else statement]
           | while ( expression ) statement
           | break ; | continue ; | ;
```

Declarations without an initializer are initialized to zero or false. Duplicate names in the same scope are errors. The function's parameters share a scope with the outermost body block. Nested blocks may shadow outer variables, but a variable cannot be read in its own initializer, including an initializer that would otherwise refer to an outer variable with the same name.

The arms of an `if` and the body of a `while` introduce a local scope even when the statement is not enclosed in braces. Variables declared inside them do not escape. A local variable may shadow a function name; calling that shadowed name is then an error. `else` belongs to the nearest unmatched `if`.

`break` and `continue` apply to the innermost loop. `continue` jumps to the loop's condition test. Both are invalid outside a loop. Return statements must match the enclosing function's type.

Return analysis is deliberately conservative. A return statement guarantees return; an `if` guarantees return only when both arms do; a sequence guarantees return if it contains a statement that guarantees return. A `while` alone never guarantees return, even if its condition is `true`. Put an explicit return after a loop when needed. Unreachable statements are still type checked.

## Comments and identifiers

Identifiers use ASCII letters, digits, and underscores, with no leading digit. Keywords cannot be identifiers. `//` comments extend through the end of the line. `/* ... */` comments may span lines and do not nest. Unterminated block comments and unexpected characters are errors.

The language deliberately differs from ISO C in its 64-bit `int`, strict boolean typing, deterministic initialization, wrapping arithmetic, and simplified grammar. It is designed to teach compiler construction rather than provide C compatibility.
