# Eon Compiler Specification
## NASM-Based Self-Hosting Compiler Implementation

### Overview
The Eon compiler is a self-hosting compiler written primarily in NASM assembly language, designed to compile Eon source code into native machine code. It features a complete toolchain including lexer, parser, semantic analyzer, optimizer, and code generator.

### Architecture

#### 1. Lexer (`eon_lexer_complete.cpp`)
- **Purpose**: Tokenizes Eon source code into a stream of tokens
- **Key Features**:
  - Python-style indentation handling (INDENT/DEDENT tokens)
  - String interpolation support (`${expression}`)
  - Multi-format numeric literals (decimal, hex, binary, float)
  - Comprehensive operator recognition
  - Comment handling (line `//` and block `/* */`)

#### 2. Pratt Parser (`eon_pratt_parser.asm`)
- **Purpose**: Parses token stream into Abstract Syntax Tree (AST)
- **Key Features**:
  - Precedence climbing algorithm for proper operator precedence
  - Left and right associativity support
  - Expression parsing with full operator support
  - Function call and member access parsing
  - Array and struct literal parsing

#### 3. Semantic Analyzer (`eon_compiler_semantic.c`)
- **Purpose**: Performs type checking and semantic analysis
- **Key Features**:
  - Type inference and checking
  - Symbol table management
  - Function signature validation
  - Variable scope resolution

#### 4. Code Generator (`eon_assembly_generator.c`)
- **Purpose**: Generates native assembly code from AST
- **Key Features**:
  - Multiple target architectures (x86-64, ARM64)
  - Register allocation
  - Instruction selection
  - Assembly output formatting

### Token Types

#### Literals
- `NUMBER`: Numeric literals (int, float, hex, binary)
- `STRING`: String literals with interpolation
- `CHAR`: Character literals
- `IDENTIFIER`: Variable and function names

#### Keywords
- `def`, `func`, `let`, `mut`, `if`, `else`, `while`, `for`, `ret`, `break`, `continue`
- `struct`, `enum`, `trait`, `impl`, `pub`, `priv`
- `true`, `false`, `null`, `self`

#### Operators
- **Arithmetic**: `+`, `-`, `*`, `/`, `%`, `^`
- **Comparison**: `==`, `!=`, `<`, `<=`, `>`, `>=`
- **Logical**: `&&`, `||`, `!`
- **Bitwise**: `&`, `|`, `~`, `<<`, `>>`
- **Assignment**: `=`, `+=`, `-=`, `*=`, `/=`
- **Special**: `->`, `=>`, `::`, `?`, `:`

#### Punctuation
- `(`, `)`, `{`, `}`, `[`, `]`, `;`, `,`, `.`, `:`

### Expression Precedence (Highest to Lowest)

1. **Primary** (15): Literals, identifiers, parentheses
2. **Postfix** (14): Function calls, member access, array indexing
3. **Unary** (13): `!`, `~`, `-`, `+`, `*`, `&`
4. **Multiplicative** (12): `*`, `/`, `%`
5. **Additive** (11): `+`, `-`
6. **Shift** (10): `<<`, `>>`
7. **Comparison** (9): `<`, `<=`, `>`, `>=`
8. **Equality** (8): `==`, `!=`
9. **Bitwise AND** (7): `&`
10. **Bitwise XOR** (6): `^`
11. **Bitwise OR** (5): `|`
12. **Logical AND** (4): `&&`
13. **Logical OR** (3): `||`
14. **Ternary** (2): `? :` (right-associative)
15. **Assignment** (1): `=`, `+=`, `-=`, `*=`, `/=` (right-associative)

### AST Node Types

#### Expression Nodes
- `BinaryExpression`: Left operand, operator, right operand
- `UnaryExpression`: Operator, operand
- `TernaryExpression`: Condition, true expression, false expression
- `AssignmentExpression`: Left side, operator, right side
- `FunctionCallExpression`: Function name, arguments
- `MemberAccessExpression`: Object, member name
- `ArrayLiteralExpression`: Elements
- `StructLiteralExpression`: Field-value pairs

#### Literal Nodes
- `NumberLiteral`: Numeric value and type
- `StringLiteral`: String value and interpolation points
- `CharLiteral`: Character value
- `Identifier`: Name and scope information

#### Statement Nodes
- `FunctionDeclaration`: Name, parameters, return type, body
- `VariableDeclaration`: Name, type, initializer
- `IfStatement`: Condition, then branch, else branch
- `WhileStatement`: Condition, body
- `ForStatement`: Initializer, condition, increment, body
- `ReturnStatement`: Expression
- `BlockStatement`: List of statements

### Build System

#### Prerequisites
- NASM (Netwide Assembler) 2.15+
- GCC (MinGW-w64 recommended)
- Windows 10/11 (x64)

#### Build Process
1. **Assembly Phase**: NASM compiles `.asm` files to object files
2. **Compilation Phase**: GCC compiles C/C++ files to object files
3. **Linking Phase**: GCC links all objects into final executable

#### Build Script
```batch
build_eon_compiler_nasm.bat
```
- Automatically checks for required tools
- Assembles all compiler components
- Links into final executable
- Tests with sample Eon program

### Usage

#### Basic Compilation
```bash
eon_compiler.exe input.eon -o output.asm
```

#### Command Line Options
- `-o <file>`: Specify output file
- `-t <target>`: Target architecture (x86-64, arm64)
- `-O <level>`: Optimization level (0-3)
- `-S`: Generate assembly only
- `-c`: Compile to object file
- `-v`: Verbose output

### Example Eon Program

```eon
def func fibonacci(n: int) -> int {
    if n <= 1 {
        ret n
    }
    ret fibonacci(n - 1) + fibonacci(n - 2)
}

def func main() -> int {
    let result = fibonacci(10)
    println("Fibonacci(10) = ${result}")
    ret 0
}
```

### Error Handling

#### Lexical Errors
- Invalid character sequences
- Unterminated strings/comments
- Invalid numeric literals

#### Syntax Errors
- Unexpected tokens
- Missing delimiters
- Invalid operator usage

#### Semantic Errors
- Type mismatches
- Undefined variables/functions
- Invalid function calls

### Optimization Features

#### Constant Folding
- Evaluate constant expressions at compile time
- Optimize arithmetic operations
- Simplify boolean expressions

#### Dead Code Elimination
- Remove unreachable code
- Eliminate unused variables
- Optimize control flow

#### Common Subexpression Elimination
- Cache repeated computations
- Optimize redundant operations
- Improve register usage

### Future Enhancements

#### Planned Features
- Generic type system
- Pattern matching
- Memory management
- Standard library
- Package system
- Debug information generation

#### Performance Improvements
- Advanced register allocation
- Instruction scheduling
- Loop optimization
- Function inlining

### Development Status

#### Completed
-  Lexer implementation
-  Pratt parser implementation
-  Basic AST structure
-  Build system
-  Documentation

#### In Progress
-  Semantic analyzer
-  Code generator
-  Error reporting

#### Planned
- ⏳ Optimizer
- ⏳ Standard library
- ⏳ Package manager
- ⏳ IDE integration

### Contributing

#### Code Style
- Use consistent indentation (4 spaces)
- Follow NASM syntax conventions
- Document all public functions
- Include error handling

#### Testing
- Test with various Eon programs
- Verify correct assembly output
- Check error handling
- Performance benchmarking

### License
This project is licensed under the MIT License. See LICENSE file for details.

### Contact
For questions, issues, or contributions, please refer to the project repository.
