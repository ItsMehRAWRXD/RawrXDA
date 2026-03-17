#!/usr/bin/env pwsh

<#
.SYNOPSIS
Convert BigDaddyG 70B model to a 7B version using layer pruning via GGUF metadata modification.

.DESCRIPTION
This script reduces a 70B parameter model to approximately 7B parameters by:
1. Using the 'gguf' python library to parse the model
2. Pruning layers (keeping 8 layers out of the total)
3. Updating metadata to reflect the new architecture
4. Copying only the necessary tensors

.PARAMETER InputModel
Path to the input 70B model

.PARAMETER OutputModel  
Path for the output 7B model
#>

param(
    [Parameter(Mandatory=$false)]
    [string]$InputModel = "D:\OllamaModels\BigDaddyG-Q2_K-CHEETAH.gguf",
    
    [Parameter(Mandatory=$false)]
    [string]$OutputModel = "D:\OllamaModels\BigDaddyG-7B-CHEETAH-V2.gguf"
)

Write-Host @"
╔══════════════════════════════════════════════╗
║  🐆 BIGDADDYG 70B → 7B MODEL CONVERTER 🐆   ║
║        GGUF Layer Pruning Utility            ║
╚══════════════════════════════════════════════╝
"@ -ForegroundColor Cyan

# Check dependencies
Write-Host "`n🔍 Checking dependencies..." -ForegroundColor Yellow
try {
    python --version | Out-Null
    Write-Host "   ✅ Python found" -ForegroundColor Green
} catch {
    Write-Host "   ❌ Python not found. Please install Python." -ForegroundColor Red
    exit 1
}

Write-Host "   📦 Installing/Updating gguf library..." -ForegroundColor Yellow
pip install gguf | Out-Null

# Create Python script
$pythonScriptPath = "$PSScriptRoot\prune_gguf.py"
Write-Host "`n📝 Creating Python pruning script at $pythonScriptPath..." -ForegroundColor Yellow

@'
import gguf
import numpy as np
import os
import re
import sys

def prune_model(input_path, output_path, target_layers=8):
    print(f"Loading model: {input_path}")
    try:
        reader = gguf.GGUFReader(input_path)
    except Exception as e:
        print(f"Error reading model: {e}")
        return False
    
    # Get current layer count
    n_layer_field = None
    for field in reader.fields.values():
        if field.name.endswith(".block_count") or field.name.endswith(".n_layer"):
            n_layer_field = field
            break
            
    if not n_layer_field:
        print("Could not find layer count in metadata")
        return False
        
    current_layers = int(n_layer_field.parts[0][0])
    print(f"Current layers: {current_layers}")
    
    if target_layers >= current_layers:
        print(f"Target layers ({target_layers}) >= current layers ({current_layers}). No pruning needed.")
        return False

    # Calculate which layers to keep
    layers_to_keep = set(np.linspace(0, current_layers - 1, target_layers, dtype=int))
    print(f"Keeping layers: {sorted(list(layers_to_keep))}")
    
    # Create mapping
    layer_map = {}
    new_idx = 0
    for i in range(current_layers):
        if i in layers_to_keep:
            layer_map[i] = new_idx
            new_idx += 1
            
    print(f"Layer mapping created. New layer count: {new_idx}")

    writer = gguf.GGUFWriter(output_path, arch="llama")
    
    print("Copying metadata...")
    
    def to_py(v):
        if isinstance(v, (np.integer, np.uint32, np.uint64, np.int32, np.int64)):
            return int(v)
        if isinstance(v, (np.floating, np.float32, np.float64)):
            return float(v)
        if isinstance(v, np.bool_):
            return bool(v)
        return v

    for key, field in reader.fields.items():
        if key == n_layer_field.name:
            continue
            
        val = field.parts[0]
        val_type = field.types[0]
        
        try:
            if val_type == gguf.GGUFValueType.UINT32:
                writer.add_uint32(key, to_py(val[0]))
            elif val_type == gguf.GGUFValueType.INT32:
                writer.add_int32(key, to_py(val[0]))
            elif val_type == gguf.GGUFValueType.FLOAT32:
                writer.add_float32(key, to_py(val[0]))
            elif val_type == gguf.GGUFValueType.BOOL:
                writer.add_bool(key, to_py(val[0]))
            elif val_type == gguf.GGUFValueType.STRING:
                writer.add_string(key, str(val[0], encoding='utf-8'))
            elif val_type == gguf.GGUFValueType.UINT64:
                writer.add_uint64(key, to_py(val[0]))
            elif val_type == gguf.GGUFValueType.INT64:
                writer.add_int64(key, to_py(val[0]))
            elif val_type == gguf.GGUFValueType.FLOAT64:
                writer.add_float64(key, to_py(val[0]))
            elif val_type == gguf.GGUFValueType.ARRAY:
                data = [to_py(x) for x in val]
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
'@ | Out-File -FilePath $pythonScriptPath -Encoding UTF8

# Run conversion
Write-Host "`n🚀 Starting conversion..." -ForegroundColor Cyan
Write-Host "   📁 Input: $InputModel" -ForegroundColor White
Write-Host "   📁 Output: $OutputModel" -ForegroundColor White

python $pythonScriptPath $InputModel $OutputModel 8

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n🎉 Conversion Complete!" -ForegroundColor Green
    
    if (Test-Path $OutputModel) {
        $size = (Get-Item $OutputModel).Length / 1GB
        Write-Host "   📏 New Size: $([math]::Round($size, 2)) GB" -ForegroundColor Green
        Write-Host "   🎯 Parameters: ~7B" -ForegroundColor Green
    }
} else {
    Write-Host "`n❌ Conversion failed" -ForegroundColor Red
}
