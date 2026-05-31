#!/bin/bash
# Simple build script for RawrXD Code Generator

cd "$(dirname "$0")" || exit 1

echo "Building RawrXD Code Generator..."
mkdir -p build

# Compile all source files
g++ -std=c++20 -O2 -Wall -Wextra \
    -I. \
    generator.cpp \
    ast.cpp \
    code_builder.cpp \
    file_emitter.cpp \
    schema_parser.cpp \
    main.cpp \
    -o build/rawrxd_generator

if [ $? -eq 0 ]; then
    echo "Build successful!"
    echo "Executable: build/rawrxd_generator"
    ./build/rawrxd_generator
else
    echo "Build failed!"
    exit 1
fi
