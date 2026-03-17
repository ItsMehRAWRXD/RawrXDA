# {{PROJECT_NAME}}

A C++ console application created with Native IDE.

## Building

To build this project:

```bash
# Using GCC
gcc -o {{PROJECT_NAME}}.exe main.cpp -static -static-libgcc -static-libstdc++

# Using Clang
clang -o {{PROJECT_NAME}}.exe main.cpp -static -static-libgcc -static-libstdc++

# Using Make (if Makefile exists)
make
```

## Running

```bash
./{{PROJECT_NAME}}.exe
```

## Features

- Cross-platform C++ console application
- Static linking for portability
- Simple user interaction example
