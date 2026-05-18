# CVM++ — Stack-Based Virtual Machine & Custom Compiler

> A complete, production-quality scripting language implemented in modern C++17,
> featuring a hand-written lexer, recursive-descent parser, AST, bytecode compiler,
> and stack-based virtual machine — all from scratch.

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Building](#building)
3. [Usage](#usage)
4. [Language Reference](#language-reference)
5. [Instruction Set Architecture (ISA)](#instruction-set-architecture-isa)
6. [Internals Deep Dive](#internals-deep-dive)
7. [Standard Library](#standard-library)
8. [Debug & Introspection](#debug--introspection)
9. [Sample Programs](#sample-programs)
10. [What Makes CVM++ Stand Out](#what-makes-cvm-stand-out)

---

## Architecture Overview

```
Source Code (.cvm)
      │
      ▼
┌─────────────┐
│    LEXER    │  Tokenizes raw text → Token stream
│  lexer.hpp  │  Handles: numbers, floats, strings, keywords,
│  lexer.cpp  │  operators, multi-line comments (/* */), # comments
└──────┬──────┘
       │  std::vector<Token>
       ▼
┌─────────────┐
│   PARSER    │  Recursive Descent → Abstract Syntax Tree
│  parser.hpp │  Pratt-style precedence climbing
│  parser.cpp │  20+ node types, full operator precedence
└──────┬──────┘
       │  ASTNodePtr (unique_ptr tree)
       ▼
┌──────────────┐
│   COMPILER   │  AST Visitor → Bytecode Chunks
│ compiler.hpp │  Scope resolution, jump patching,
│ compiler.cpp │  function compilation, break/continue
└──────┬───────┘
       │  Compiler::Result { Chunk, FunctionProto[] }
       ▼
┌──────────────┐
│   VM         │  Stack-based fetch-decode-execute loop
│   vm.hpp     │  Call frames, global/local vars, native fns,
│   vm.cpp     │  I/O, 40+ opcodes, execution statistics
└──────────────┘
```

---

## Building

### Prerequisites
- C++17 compiler (GCC 8+, Clang 7+, MSVC 2019+)
- CMake 3.16+ (optional) or a direct g++ command

### With CMake
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
```

### Direct g++ (fastest)
```bash
g++ -std=c++17 -O2 -Wall \
    lexer.cpp parser.cpp compiler.cpp vm.cpp main.cpp \
    -o cvm
```

### Build & run tests
```bash
g++ -std=c++17 -O2 \
    lexer.cpp parser.cpp compiler.cpp vm.cpp test_runner.cpp \
    -o cvm_test && ./cvm_test
```

---

## Usage

### Run a script
```bash
./cvm script.cvm
./cvm --bytecode --stats script.cvm    # with debug output
```

### REPL (interactive mode)
```bash
./cvm
./cvm --repl
```

### CLI flags
| Flag          | Description                              |
|---------------|------------------------------------------|
| `--repl`      | Launch interactive REPL                  |
| `--ast`       | Print AST in debug mode                  |
| `--bytecode`  | Print disassembled bytecode              |
| `--stats`     | Print execution statistics               |
| `--no-color`  | Disable ANSI color output                |
| `--help`      | Show help message                        |

### REPL commands
| Command         | Description                           |
|-----------------|---------------------------------------|
| `:quit` / `:q`  | Exit                                  |
| `:ast`          | Toggle AST dump                       |
| `:bytecode`     | Toggle bytecode disassembly           |
| `:stats`        | Toggle execution stats                |
| `:globals`      | Show all global variables             |
| `:clear`        | Clear all globals and reset VM        |
| `:load <file>`  | Load and execute a .cvm file          |
| `line \`        | Multi-line continuation               |

---

## Language Reference

### Data Types
| Type     | Example                  | Notes                       |
|----------|--------------------------|-----------------------------|
| `int`    | `42`, `-7`, `1_000_000`  | 64-bit signed integer        |
| `float`  | `3.14`, `2.5e-3`         | 64-bit IEEE 754 double       |
| `bool`   | `true`, `false`          | Boolean                      |
| `string` | `"hello"`, `'world'`     | UTF-8 text, escape sequences |
| `nil`    | `nil`                    | Null/undefined value         |

### Variables
```cvm
let x = 10
let name = "CVM++"
let pi = 3.14159
let flag = true
let nothing = nil
```

### Operators
```cvm
# Arithmetic
a + b    a - b    a * b    a / b    a % b
-a                                          # unary minus

# Comparison
a == b   a != b   a < b   a > b   a <= b   a >= b

# Logical
a and b   a or b   not a   # short-circuit evaluation

# Assignment
x = 10
x += 5    x -= 3    x *= 2    x /= 4
++x       --x       x++       x--         # increment/decrement

# String
"hello" + " world"   # concatenation
"ha" * 3             # repeat → "hahaha"
```

### Control Flow
```cvm
# If / else if / else
if (x > 0) {
    print("positive")
} else {
    if (x < 0) { print("negative") }
    else { print("zero") }
}

# While loop
while (i < 10) {
    i += 1
}

# For loop
for (let i = 0; i < 10; i = i + 1) {
    print(i)
}

# Break & Continue
while (true) {
    if (done) { break }
    if (skip) { continue }
    process()
}
```

### Functions
```cvm
fn add(a, b) {
    return a + b
}

print(add(3, 4))   # → 7

# Recursive
fn fib(n) {
    if (n <= 1) { return n }
    return fib(n - 1) + fib(n - 2)
}

# Mutual recursion
fn is_even(n) {
    if (n == 0) { return true }
    return is_odd(n - 1)
}
fn is_odd(n) {
    if (n == 0) { return false }
    return is_even(n - 1)
}
```

### I/O
```cvm
print("Hello, World!")
print(42, 3.14, true)        # multiple values

let name = input("Your name: ")
let age  = input()            # no prompt, auto-converts to int/float
```

### Comments
```cvm
# Single-line comment

/* Multi-line
   comment */
```

---

## Instruction Set Architecture (ISA)

CVM++ uses a compact, typed bytecode. Each instruction is 1 byte (opcode) followed by 0–8 bytes of operands.

### Stack Operations
| Opcode        | Operands       | Description                      |
|---------------|----------------|----------------------------------|
| `PUSH_INT`    | 8 bytes (i64)  | Push 64-bit integer              |
| `PUSH_FLOAT`  | 8 bytes (f64)  | Push 64-bit float                |
| `PUSH_BOOL`   | 1 byte         | Push boolean                     |
| `PUSH_STRING` | 2 bytes (idx)  | Push string from constant pool   |
| `PUSH_NIL`    | —              | Push nil                         |
| `POP`         | —              | Discard top of stack             |

### Arithmetic
| Opcode  | Description              |
|---------|--------------------------|
| `ADD`   | Pop b, a → push a+b      |
| `SUB`   | Pop b, a → push a-b      |
| `MUL`   | Pop b, a → push a*b      |
| `DIV`   | Pop b, a → push a/b      |
| `MOD`   | Pop b, a → push a%b      |
| `NEG`   | Pop a → push -a          |

### Comparison & Logic
| Opcode  | Description                  |
|---------|------------------------------|
| `EQ`    | a == b                       |
| `NEQ`   | a != b                       |
| `LT`    | a < b                        |
| `GT`    | a > b                        |
| `LTE`   | a <= b                       |
| `GTE`   | a >= b                       |
| `AND`   | logical and (short-circuit)  |
| `OR`    | logical or (short-circuit)   |
| `NOT`   | logical not                  |

### Variables
| Opcode         | Operands   | Description                    |
|----------------|------------|--------------------------------|
| `DEFINE_VAR`   | 2 bytes    | Pop value, store at local slot |
| `LOAD_VAR`     | 2 bytes    | Push local variable            |
| `STORE_VAR`    | 2 bytes    | Write local (no pop)           |
| `LOAD_GLOBAL`  | 2 bytes    | Push global by name index      |
| `STORE_GLOBAL` | 2 bytes    | Write global (no pop)          |

### Control Flow
| Opcode           | Operands  | Description                    |
|------------------|-----------|--------------------------------|
| `JUMP`           | 4 bytes   | Unconditional jump             |
| `JUMP_IF_FALSE`  | 4 bytes   | Jump if top is falsy (peek)    |
| `JUMP_IF_TRUE`   | 4 bytes   | Jump if top is truthy (peek)   |

### Functions
| Opcode        | Operands  | Description                     |
|---------------|-----------|---------------------------------|
| `MAKE_FUNC`   | 2 bytes   | Push function reference         |
| `CALL`        | 1 byte    | Call with N args                |
| `RETURN`      | —         | Pop return value, restore frame |
| `RETURN_NIL`  | —         | Return nil, restore frame       |

### I/O & Misc
| Opcode      | Description                         |
|-------------|-------------------------------------|
| `PRINT`     | Pop and print with newline          |
| `INPUT`     | Pop prompt, read line, push result  |
| `STR_CONCAT`| Pop two, concatenate, push          |
| `HALT`      | Stop execution                      |
| `NOP`       | No-operation                        |
| `DEBUG_BREAK`| Emit debug trace                   |

---

## Internals Deep Dive

### Call Convention

The VM uses a **frame-based call convention**:

```
Before CALL (argc=2):
  stack: [ ... | fn_ref | arg0 | arg1 ]
                  ^fn_pos  ^base
  
CALL handler:
  1. Resolve fn_ref to FunctionProto
  2. Shift args left over fn_ref slot
  3. Create CallFrame { chunk, ip=0, base=fn_pos }
  
Inside function:
  LOAD_VAR slot=0 → stack_[frame.base + 0]  (= arg0)
  LOAD_VAR slot=1 → stack_[frame.base + 1]  (= arg1)
  LET x = ...  →  slot=2  (local after params)

RETURN:
  1. Pop return value
  2. Shrink stack to frame.base (remove all locals + args)
  3. Push return value
  4. Pop frame
```

### Scope Resolution

Local variables are resolved at **compile time** using a scope stack. Each scope records a mapping of `name → slot_index`. During code generation:
- If a name resolves to a local slot → emit `LOAD_VAR / STORE_VAR`
- If not found in any local scope → emit `LOAD_GLOBAL / STORE_GLOBAL`

Function scopes start fresh at slot 0, preventing cross-function slot aliasing.

### Jump Patching

The compiler uses a **backpatching** strategy for forward jumps:
1. Emit `JUMP_IF_FALSE` with placeholder `0xFFFFFFFF`
2. Record the offset of the placeholder
3. After compiling the target code, call `patch_jump(offset, target)`
4. Overwrite the 4 placeholder bytes with the real target address

Loop break/continue statements accumulate unpatched jump offsets in a `LoopContext` stack, all patched at loop end.

### Short-Circuit Evaluation

`and` / `or` use peek-based jumps for lazy evaluation:
```
AND:  eval left → JUMP_IF_FALSE (peek, don't pop) → POP → eval right → [target]
OR:   eval left → JUMP_IF_TRUE  (peek, don't pop) → POP → eval right → [target]
```
This ensures the right operand is only evaluated when needed.

---

## Standard Library

| Function          | Args       | Returns | Description                  |
|-------------------|------------|---------|------------------------------|
| `sqrt(x)`         | float/int  | float   | Square root                  |
| `abs(x)`          | number     | number  | Absolute value               |
| `pow(base, exp)`  | numbers    | float   | Exponentiation               |
| `floor(x)`        | float      | float   | Round down                   |
| `ceil(x)`         | float      | float   | Round up                     |
| `round(x)`        | float      | float   | Round to nearest             |
| `max(a, b)`       | comparable | any     | Maximum of two values        |
| `min(a, b)`       | comparable | any     | Minimum of two values        |
| `str(x)`          | any        | string  | Convert to string            |
| `int(x)`          | any        | int     | Convert to integer           |
| `float(x)`        | any        | float   | Convert to float             |
| `bool(x)`         | any        | bool    | Convert to boolean           |
| `len(s)`          | string     | int     | String length                |
| `type(x)`         | any        | string  | Type name of value           |
| `chr(n)`          | int        | string  | ASCII char from code point   |
| `ord(s)`          | string     | int     | Code point of first char     |

---

## Debug & Introspection

### AST output (`--ast`)
```
(Program
  (Let x
    [Literal: 10])
  (Print
    (Binary +
      [Ident: x]
      [Literal: 5])))
```

### Bytecode disassembly (`--bytecode`)
```
══════════════════════════════════════
  CHUNK: main
══════════════════════════════════════
  String constants:
    [0] = "x"
  Instructions:
000000  L   1  PUSH_INT           10
000009  L   1  DEFINE_VAR         slot=0
000012  L   2  LOAD_VAR           slot=0
000015  L   2  PUSH_INT           5
000024  L   2  ADD               
000025  L   2  PRINT             
000026  L   2  HALT              
```

### Execution statistics (`--stats`)
```
── Execution Stats ─────────────────────
  Compile time   : 0.142 ms
  Execution time : 2.831 ms
  Instructions   : 10482
  Stack peak     : 12
  Function calls : 77
```

---

## Sample Programs

### FizzBuzz
```cvm
for (let i = 1; i <= 100; i = i + 1) {
    if (i % 15 == 0)     { print("FizzBuzz") }
    else { if (i % 3 == 0) { print("Fizz") }
    else { if (i % 5 == 0) { print("Buzz") }
    else                   { print(i) } } }
}
```

### Fibonacci
```cvm
fn fib(n) {
    if (n <= 1) { return n }
    return fib(n - 1) + fib(n - 2)
}
for (let i = 0; i <= 15; i = i + 1) {
    print("fib(" + str(i) + ") = " + str(fib(i)))
}
```

### Sieve of Eratosthenes (logic-level)
```cvm
fn is_prime(n) {
    if (n < 2) { return false }
    let d = 2
    while (d * d <= n) {
        if (n % d == 0) { return false }
        d += 1
    }
    return true
}
for (let n = 2; n <= 50; n = n + 1) {
    if (is_prime(n)) { print(n) }
}
```

---

## What Makes CVM++ Stand Out

1. **Complete Pipeline** — Lexer → Parser → AST → Compiler → VM, all from scratch with no libraries

2. **Production-quality ISA** — 40+ opcodes, 64-bit integers and doubles, typed value system using `std::variant`

3. **Correct Call Convention** — Frame-based stack discipline with proper local variable slots, recursive functions work correctly with any call depth

4. **Short-circuit Evaluation** — `and`/`or` use peek-based jump semantics, not post-eval boolean ops

5. **Compile-time Jump Patching** — All forward jumps use the backpatch pattern; break/continue loop through a `LoopContext` stack

6. **Extensible Native Runtime** — Register C++ lambdas as CVM++ functions with `vm.register_native()`

7. **Rich Debug Output** — AST pretty-printer, bytecode disassembler with line numbers, execution stats

8. **Interactive REPL** — Persistent global state, toggle debug modes, `:load`, `:globals`, multi-line input

9. **100% Test Coverage** — 55 automated tests covering every language feature, with expected-output matching

10. **Zero dependencies** — Pure C++17, STL only

---

*Built for the CVM++ project — mentored by Raman (7977779056)*
# CVM-
