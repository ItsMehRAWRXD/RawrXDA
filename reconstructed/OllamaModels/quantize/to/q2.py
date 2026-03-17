#!/usr/bin/env python3
"""
Quantize BigDaddyG-F32-FROM-Q4.gguf to Q2_K format
"""

import os
import sys
import subprocess

# Paths
input_model = r"D:\OllamaModels\BigDaddyG-F32-FROM-Q4.gguf"
output_model = r"D:\OllamaModels\BigDaddyG-F32-Q2_K.gguf"

# Check if llama.cpp quantize exists
llama_cpp_paths = [
    r"D:\OllamaModels\llama.cpp\quantize.exe",
    r"D:\OllamaModels\llama.cpp\build\bin\Release\quantize.exe",
    r"C:\Program Files\llama.cpp\quantize.exe",
    "quantize.exe"
]

quantize_exe = None
for path in llama_cpp_paths:
    if os.path.exists(path):
        quantize_exe = path
        break

if not quantize_exe:
    print("ERROR: llama.cpp quantize tool not found!")
    print("\nTo quantize this model, you need llama.cpp installed.")
    print("\nOptions:")
    print("1. Download llama.cpp from: https://github.com/ggerganov/llama.cpp")
    print("2. Build it with: cmake -B build && cmake --build build --config Release")
    print("3. Or download pre-built binaries")
    print("\nAlternatively, I'll create the Modelfile that references the existing model")
    print("and you can manually quantize later.")
    sys.exit(1)

# Run quantization
print(f"Quantizing {input_model} to Q2_K format...")
print(f"Output: {output_model}")

cmd = [quantize_exe, input_model, output_model, "Q2_K"]
result = subprocess.run(cmd, capture_output=True, text=True)

if result.returncode == 0:
    print("\n✓ Quantization successful!")
    print(f"Created: {output_model}")
else:
    print("\n✗ Quantization failed!")
    print(result.stderr)
    sys.exit(1)
