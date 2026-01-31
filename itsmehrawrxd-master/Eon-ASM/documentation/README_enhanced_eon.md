# Enhanced EON Compiler v2.0

A comprehensive, production-ready compiler for the EON programming language, implementing all advanced features from the rwararw.txt specification.

## Features

###  **Complete Language Support**
- **Variables & Types**: `let`, `const`, type annotations, automatic type inference
- **Functions**: Full function definitions with parameters, return types, and recursion
- **Structs**: User-defined data structures with member access
- **Pointers**: Memory management with `*`, `&`, `alloc`, `free`, `sizeof`
- **Control Flow**: `if/else`, `loop`, `break`, `continue`
- **Concurrency**: `spawn`, `shared`, `mutex`, `lock`, `unlock`

###  **Advanced Compiler Features**
- **Comprehensive Lexer**: 50+ token types with proper error reporting
- **Recursive Descent Parser**: Handles operator precedence and associativity
- **Semantic Analysis**: Type checking, scope resolution, symbol table management
- **Intermediate Representation**: Three-Address Code (TAC) generation
- **Code Generation**: x86-64 assembly with proper calling conventions
- **Optimization**: Constant folding, dead code elimination

###  **Compiler Architecture**

```
Source Code → Lexer → Parser → AST → Semantic Analysis → IR → Optimization → Assembly
```

## Quick Start

### Prerequisites
```bash
# Ubuntu/Debian
sudo apt-get install gcc nasm make

# macOS
brew install gcc nasm

# Windows (WSL)
# Use Ubuntu/Debian commands above
```

### Build and Test
```bash
# Build the compiler
make -f Makefile.enhanced all

# Run comprehensive test
make -f Makefile.enhanced test

# Clean up
make -f Makefile.enhanced clean
```

## Language Syntax

### Variables and Types
```eon
let x = 5;                    // Integer variable
let y: int = 10;             // Explicit type annotation
const PI = 3.14159;          // Constant
let ptr: int* = alloc(8);    // Pointer type
```

### Functions
```eon
def func add(a: int, b: int) -> int {
    ret a + b;
}

def func factorial(n: int) -> int {
    if (n > 1) {
        ret n * factorial(n - 1);
    } else {
        ret 1;
    }
}
```

### Structs
```eon
model Point {
    x: int
    y: int
}

def func create_point(x: int, y: int) -> Point {
    let p: Point = alloc(sizeof(Point));
    p.x = x;
    p.y = y;
    ret p;
}
```

### Control Flow
```eon
// If statements
if (x > 0) {
    print("Positive");
} else {
    print("Non-positive");
}

// Loops
let i = 0;
loop (i < 10) {
    print(i);
    i = i + 1;
}
```

### Memory Management
```eon
let ptr = alloc(8);          // Allocate 8 bytes
*ptr = 42;                   // Dereference and assign
let value = *ptr;            // Dereference and read
let addr = &value;           // Get address
let size = sizeof(Point);    // Get size
free(ptr);                   // Free memory
```

### Concurrency
```eon
shared let counter = 0;
let mutex = create_mutex();

spawn {
    lock(mutex);
    counter = counter + 1;
    unlock(mutex);
};
```

## Compiler Phases

### 1. Lexical Analysis
- Tokenizes source code into 50+ token types
- Handles keywords, identifiers, numbers, strings, operators
- Provides detailed error reporting with line/column information

### 2. Parsing
- Recursive descent parser with proper operator precedence
- Builds Abstract Syntax Tree (AST)
- Handles all language constructs including nested expressions

### 3. Semantic Analysis
- Type checking and validation
- Scope resolution with nested scopes
- Symbol table management
- Function signature validation

### 4. Intermediate Representation
- Generates Three-Address Code (TAC)
- Optimizes for code generation
- Handles complex control flow

### 5. Code Generation
- Produces x86-64 assembly
- Implements proper calling conventions
- Handles stack management and register allocation

### 6. Optimization
- Constant folding
- Dead code elimination
- Expression simplification

## Example Programs

### Hello World
```eon
def func main() -> int {
    print("Hello, World!");
    ret 0;
}
```

### Fibonacci
```eon
def func fibonacci(n: int) -> int {
    if (n <= 1) {
        ret n;
    } else {
        ret fibonacci(n - 1) + fibonacci(n - 2);
    }
}

def func main() -> int {
    let result = fibonacci(10);
    print(result);
    ret 0;
}
```

### Array Processing
```eon
def func sum_array(arr: int, len: int) -> int {
    let sum = 0;
    let i = 0;
    loop (i < len) {
        sum = sum + arr[i];
        i = i + 1;
    }
    ret sum;
}
```

## Build System

The enhanced compiler includes a comprehensive Makefile with the following targets:

- `make all` - Build the compiler
- `make test` - Build and test with sample program
- `make run-test` - Compile, assemble, link and run
- `make clean` - Remove generated files
- `make help` - Show help information

## Error Handling

The compiler provides detailed error messages including:
- Line and column numbers
- Clear error descriptions
- Context information
- Suggestions for fixes

Example error output:
```
Error: Undefined variable 'x' at line 15, column 8
Error: Type mismatch for variable 'y' at line 23, column 12
Error: Function 'foo' expects 2 arguments, got 3 at line 45, column 5
```

## Performance

The enhanced EON compiler is optimized for:
- **Fast compilation**: Efficient parsing and code generation
- **Small output**: Optimized assembly generation
- **Memory efficiency**: Minimal memory usage during compilation
- **Error recovery**: Robust error handling and reporting

## Architecture Support

- **x86-64**: Full support with proper calling conventions
- **Linux**: Primary target platform
- **macOS**: Compatible with minor modifications
- **Windows**: Compatible via WSL or MinGW

## Contributing

The enhanced EON compiler is based on the comprehensive specification in `rwararw.txt`. To contribute:

1. Review the specification in `rwararw.txt`
2. Implement missing features
3. Add comprehensive tests
4. Ensure compatibility with existing code

## License

This enhanced EON compiler implementation is part of the RawrZApp project and follows the same licensing terms.

## Acknowledgments

- Based on the comprehensive EON compiler specification in `rwararw.txt`
- Implements advanced compiler techniques and optimizations
- Includes production-ready error handling and reporting
- Supports the complete EON language feature set

---

**Enhanced EON Compiler v2.0** - A complete, production-ready compiler for the EON programming language.
