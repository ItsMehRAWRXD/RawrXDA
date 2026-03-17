import gguf
import sys
import numpy as np
import re

def to_py(val):
    if isinstance(val, np.generic):
        return val.item()
    return val

def prune_model(input_path, output_path, target_layers=8):
    print(f"Loading model from {input_path}...")
    reader = gguf.GGUFReader(input_path, 'r')
    
    writer = gguf.GGUFWriter(output_path, "llama")
    
    # Map layers
    n_layer_field = None
    for field in reader.fields.values():
        if field.name == "llama.block_count":
            n_layer_field = field
            break
            
    if not n_layer_field:
        print("Error: Could not find llama.block_count")
        return

    original_layers = int(n_layer_field.parts[-1][0])
    print(f"Original layers: {original_layers}")
    
    # Calculate layers to keep
    if target_layers >= original_layers:
        print("Target layers >= original layers, copying all")
        layers_to_keep = list(range(original_layers))
    else:
        step = original_layers / target_layers
        layers_to_keep = [int(i * step) for i in range(target_layers)]
        
    print(f"Keeping layers: {layers_to_keep}")
    
    layer_map = {old: new for new, old in enumerate(layers_to_keep)}
    
    print("Copying metadata...")
    for key, field in reader.fields.items():
        # Skip internal fields
        if key.startswith("GGUF."):
            continue
            
        # Skip block count as we'll write it manually
        if key == "llama.block_count":
            continue
            
        val_type = field.types[0]
        val_array = field.parts[-1]
        
        try:
            if val_type == gguf.GGUFValueType.UINT32:
                writer.add_uint32(key, to_py(val_array[0]))
            elif val_type == gguf.GGUFValueType.INT32:
                writer.add_int32(key, to_py(val_array[0]))
            elif val_type == gguf.GGUFValueType.FLOAT32:
                writer.add_float32(key, to_py(val_array[0]))
            elif val_type == gguf.GGUFValueType.BOOL:
                writer.add_bool(key, bool(val_array[0]))
            elif val_type == gguf.GGUFValueType.STRING:
                s = val_array.tobytes().decode('utf-8', errors='replace')
                
                # Fix architecture if needed
                if key == "general.architecture" and s != "llama":
                    print(f"Warning: Changing architecture from {s} to llama")
                    s = 'llama'
                    
                writer.add_string(key, s)
            elif val_type == gguf.GGUFValueType.UINT64:
                writer.add_uint64(key, to_py(val_array[0]))
            elif val_type == gguf.GGUFValueType.INT64:
                writer.add_int64(key, to_py(val_array[0]))
            elif val_type == gguf.GGUFValueType.FLOAT64:
                writer.add_float64(key, to_py(val_array[0]))
            elif val_type == gguf.GGUFValueType.ARRAY:
                item_type = field.types[1]
                data = []
                
                # Data starts at index 5 for arrays
                # Header: [KeyLen, Key, Type, ItemType, Count] -> 5 parts
                start_index = 5
                
                if item_type == gguf.GGUFValueType.STRING:
                    # Strings are [len, bytes] pairs in parts
                    i = start_index
                    while i < len(field.parts):
                        if i + 1 >= len(field.parts): break
                        s_bytes = field.parts[i+1]
                        data.append(s_bytes.tobytes().decode('utf-8', errors='replace'))
                        i += 2
                else:
                    # Scalars are [val] in parts
                    for i in range(start_index, len(field.parts)):
                        data.append(to_py(field.parts[i][0]))
                        
                writer.add_array(key, data)
        except Exception as e:
            print(f"Warning: Failed to copy metadata {key}: {e}")

    writer.add_uint32(n_layer_field.name, target_layers)

    print("Processing tensors...")
    layer_pattern = re.compile(r"^blk\.(\d+)\.(.*)$")
    
    for tensor in reader.tensors:
        name = tensor.name
        match = layer_pattern.match(name)
        
        if match:
            layer_idx = int(match.group(1))
            suffix = match.group(2)
            
            if layer_idx in layers_to_keep:
                new_layer_idx = layer_map[layer_idx]
                new_name = f"blk.{new_layer_idx}.{suffix}"
                writer.add_tensor(new_name, tensor.data, raw_dtype=tensor.tensor_type)
        else:
            writer.add_tensor(name, tensor.data, raw_dtype=tensor.tensor_type)

    print(f"Writing output to {output_path}...")
    writer.write_header_to_file()
    writer.write_kv_data_to_file()
    writer.write_tensors_to_file()
    writer.close()
    print("Done!")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python prune_gguf.py <input> <output> [target_layers]")
        sys.exit(1)
        
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    layers = int(sys.argv[3]) if len(sys.argv) > 3 else 8
    
    prune_model(input_file, output_file, layers)
