# RawrZApp Documentation

## Overview
RawrZApp is a bootstrapped programming language compiler suite featuring two languages:
- **Reverser**: A backwards-processing language 
- **Eon**: A modern systems language

## Quick Start
```bash
./build.sh        # Build everything
./build.sh -T     # Build and run tests
cd build && ./eon_compiler test_program.eon
```

## Languages

### Reverser Language
```reverser
// Keywords are backwards
ledom Person {     // model = ledom
    name: *char
    age: int  
}

cnuf main() -> int {  // func = cnuf
    tel p: Person     // let = tel
    ter 0            // ret = ter
}
```

### Eon Language  
```eon
def model Person {
    name: String
    age: int
}

def func main() -> int {
    let p: Person = Person{name: "Alice", age: 30}
    ret 0
}
```

## Architecture

### Bootstrapping Stages
1. **Stage 0**: Assembly bootstrap (reverser_*.asm files)
2. **Stage 1**: C bootstrap (eon_compiler.c, eon_llvm_compiler.c)  
3. **Stage 2**: Self-hosting (*_compiler.eon, *_compiler.rev)
4. **Stage 3**: Production ready

### Key Components
- **Lexers**: Token recognition and parsing
- **Parsers**: AST construction with error recovery
- **Runtime**: Stack/heap management, GC, vtables
- **Codegen**: Assembly/LLVM IR generation
- **Platform**: Cross-platform support (Linux/Windows/macOS)
- **Testing**: Comprehensive test framework
- **Build**: CMake-based build system

## Build System
```bash
# Build options
./build.sh -t Debug -T     # Debug build with tests
./build.sh --static -p     # Static build with packaging
./build.sh --asan -T       # AddressSanitizer build

# CMake options
-DBUILD_TESTS=ON
-DENABLE_ASAN=ON
-DSTATIC_BUILD=ON
```

## Testing
```bash
# Run all tests
./build.sh -T

# Individual test executables
./reverser_lexer      # Lexer tests
./reverser_parser     # Parser tests  
./reverser_runtime_test  # Runtime tests
```

## Cross-Platform Support
| Platform | Arch | Status |
|----------|------|--------|
| Linux | x86-64 |  |
| Linux | ARM64 |  |
| Windows | x86-64 |  |
| macOS | x86-64/ARM64 |  |

## Files Overview
- `reverser_*.asm`: Reverser language implementation
- `eon_*.c`: Eon compiler in C
- `*_compiler.eon/.rev`: Self-hosted compilers
- `test_*.asm`: Test suites
- `CMakeLists.txt`: Build configuration
- `build.sh/.bat`: Build scripts

## Contributing
1. Fork repository
2. Create feature branch
3. Add tests for new features  
4. Ensure cross-platform compatibility
5. Submit pull request

## License
MIT License - see LICENSE file for details.
