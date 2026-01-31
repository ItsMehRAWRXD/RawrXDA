#!/bin/bash
# EON Bootstrap Compiler
# Reconstructed from fragment analysis

echo " EON Bootstrap Compiler - Reconstructing from Fragment"

# Compile the lexer
echo " Compiling lexer..."
gcc -c components/lexer.c -o build/lexer.o

# Compile the parser  
echo " Compiling parser..."
gcc -c components/parser.c -o build/parser.o

# Compile the semantic analyzer
echo " Compiling semantic analyzer..."
gcc -c components/semantic.c -o build/semantic.o

# Compile the optimizer
echo " Compiling optimizer..."
gcc -c components/optimizer.c -o build/optimizer.o

# Compile the code generator
echo " Compiling code generator..."
gcc -c components/codegen.c -o build/codegen.o

# Link everything together
echo " Linking EON compiler..."
gcc build/*.o -o eon-compiler

echo " EON Compiler reconstructed and ready!"
echo "Usage: ./eon-compiler input.eon output.asm"