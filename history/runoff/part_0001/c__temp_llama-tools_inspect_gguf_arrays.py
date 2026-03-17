import gguf
import sys
import numpy as np

def inspect_arrays(input_path):
    print(f"Loading {input_path}...")
    reader = gguf.GGUFReader(input_path, 'r')
    
    keys_to_inspect = [
        "tokenizer.ggml.scores",
        "tokenizer.ggml.token_type",
        "tokenizer.ggml.tokens"
    ]
    
    for key in keys_to_inspect:
        if key in reader.fields:
            field = reader.fields[key]
            print(f"\nKey: {key}")
            print(f"Type: {field.types}")
            print(f"Parts length: {len(field.parts)}")
            
            # Print first few parts to understand structure
            for i in range(min(10, len(field.parts))):
                part = field.parts[i]
                val_str = str(part)
                if len(val_str) > 50: val_str = val_str[:50] + "..."
                print(f"Part {i}: type={type(part)}, shape={part.shape if hasattr(part, 'shape') else 'N/A'}, val={val_str}")

if __name__ == "__main__":
    inspect_arrays(sys.argv[1])
