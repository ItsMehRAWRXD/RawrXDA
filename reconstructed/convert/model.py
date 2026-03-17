#!/usr/bin/env python3
"""
Convert Ollama GGUF blob to Q5_K quantization.
This uses the GGUF library which is pure Python and doesn't require compilation.
"""

import struct
import shutil
from pathlib import Path

# Ollama blob path (the IQ4_NL quantized model that doesn't work)
BLOB_PATH = r"C:\Users\HiH8e\.ollama\blobs\blobs\sha256-68bbe6dc9cf42eb60c9a7f96137fb8d472f752de6ebf53e9942f267f1a1e2577"
OUTPUT_PATH = r"D:\unlocked-350m-converted.gguf"
OLLAMA_MODELS_PATH = r"D:\OllamaModels\unlocked-350M"

# Simple approach: just copy the blob as-is for now
# The real conversion would require the quantization algorithms
# But copying lets us verify the file format works

if __name__ == "__main__":
    blob_path = Path(BLOB_PATH)
    output_path = Path(OUTPUT_PATH)
    models_dir = Path(OLLAMA_MODELS_PATH)
    
    print(f"Source blob: {blob_path}")
    print(f"Blob exists: {blob_path.exists()}")
    
    if blob_path.exists():
        # Check blob size and magic bytes
        with open(blob_path, "rb") as f:
            magic = f.read(4)
            print(f"Magic bytes: {magic.hex()} = {magic}")
            f.seek(0)
            file_size = len(f.read())
            print(f"File size: {file_size / (1024**3):.2f} GB")
    
    # Create output directory
    models_dir.mkdir(parents=True, exist_ok=True)
    
    # For now, just symlink or copy metadata-only version
    # Real solution needs full llama.cpp quantize tool
    print("\nNote: Full quantization requires llama-quantize tool from llama.cpp")
    print(f"Attempted conversion path: {output_path}")
    print(f"Target models directory: {models_dir}")
