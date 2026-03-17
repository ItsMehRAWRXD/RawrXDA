#!/usr/bin/env python3
"""
GGUF Test File Generator for RawrXD Analyzer
Generates minimal valid GGUF v3 files for testing
"""

import struct
import sys
from pathlib import Path

def write_string(f, s: str):
    """Write GGUF string (uint64 length + UTF-8 bytes)"""
    encoded = s.encode('utf-8')
    f.write(struct.pack('<Q', len(encoded)))
    f.write(encoded)

def write_uint32_value(f, value: int):
    """Write uint32 metadata value"""
    f.write(struct.pack('<I', 4))  # type = UINT32
    f.write(struct.pack('<I', value))

def write_string_value(f, value: str):
    """Write string metadata value"""
    f.write(struct.pack('<I', 8))  # type = STRING
    write_string(f, value)

def create_test_gguf(output_path: Path):
    """Create minimal test GGUF file with 3 tensors"""
    
    with open(output_path, 'wb') as f:
        # Header (16 bytes)
        f.write(struct.pack('<I', 0x46554747))  # magic "GGUF"
        f.write(struct.pack('<I', 3))            # version
        f.write(struct.pack('<Q', 3))            # tensor_count (3 tensors)
        f.write(struct.pack('<Q', 2))            # metadata_kv_count (2 KV pairs)
        
        # Metadata KV section (2 pairs)
        # KV 1: "model.name" = "test-model"
        write_string(f, "model.name")
        write_string_value(f, "test-model")
        
        # KV 2: "model.layers" = 12
        write_string(f, "model.layers")
        write_uint32_value(f, 12)
        
        # Tensor info section (3 tensors)
        
        # Tensor 1: "model.embed.weight" - Embedding
        write_string(f, "model.embed.weight")
        f.write(struct.pack('<I', 2))  # ndims = 2
        f.write(struct.pack('<Q', 50257))  # dim[0] = vocab_size
        f.write(struct.pack('<Q', 768))    # dim[1] = hidden_dim
        f.write(struct.pack('<I', 0))   # dtype = F32
        f.write(struct.pack('<Q', 0))   # offset = 0
        
        # Tensor 2: "model.layers.0.attn.q_proj.weight" - Attention
        write_string(f, "model.layers.0.attn.q_proj.weight")
        f.write(struct.pack('<I', 2))  # ndims = 2
        f.write(struct.pack('<Q', 768))   # dim[0]
        f.write(struct.pack('<Q', 768))   # dim[1]
        f.write(struct.pack('<I', 0))   # dtype = F32
        f.write(struct.pack('<Q', 38597376))  # offset
        
        # Tensor 3: "model.layers.0.ffn.gate_proj.weight" - FFN
        write_string(f, "model.layers.0.ffn.gate_proj.weight")
        f.write(struct.pack('<I', 2))  # ndims = 2
        f.write(struct.pack('<Q', 768))   # dim[0]
        f.write(struct.pack('<Q', 3072))  # dim[1]
        f.write(struct.pack('<I', 0))   # dtype = F32
        f.write(struct.pack('<Q', 40960000))  # offset
        
        # Alignment (GGUF requires 32-byte alignment for tensor data)
        current_pos = f.tell()
        alignment = 32
        padding = (alignment - (current_pos % alignment)) % alignment
        f.write(b'\x00' * padding)
        
        # Tensor data (dummy - just zeros)
        # Total parameters:
        # embed: 50257 * 768 = 38,597,376
        # attn:  768 * 768 = 589,824
        # ffn:   768 * 3072 = 2,359,296
        # Total: 41,546,496 parameters
        
        tensor_data_size = (50257 * 768 + 768 * 768 + 768 * 3072) * 4  # F32 = 4 bytes
        # Don't write actual data (would be 166MB) - analyzer only needs metadata
        
    file_size = output_path.stat().st_size
    print(f"✅ Created test GGUF file: {output_path}")
    print(f"   Size: {file_size:,} bytes")
    print(f"   Tensors: 3 (embed, attn, ffn)")
    print(f"   Expected parameters: 41,546,496")
    print(f"   Expected counts:")
    print(f"     - Attention: 1")
    print(f"     - FFN: 1")
    print(f"     - Embedding: 1")
    print(f"     - Normalization: 0")

def create_large_test_gguf(output_path: Path, num_layers: int = 32):
    """Create realistic multi-layer GGUF file"""
    
    with open(output_path, 'wb') as f:
        # Calculate tensor count
        # Per layer: q, k, v, o, gate, up, down, attn_norm, ffn_norm = 9 tensors
        # Plus: embed, output, final_norm = 3 tensors
        tensor_count = num_layers * 9 + 3
        
        # Header
        f.write(struct.pack('<I', 0x46554747))
        f.write(struct.pack('<I', 3))
        f.write(struct.pack('<Q', tensor_count))
        f.write(struct.pack('<Q', 3))  # 3 metadata KV pairs
        
        # Metadata
        write_string(f, "model.name")
        write_string_value(f, f"test-{num_layers}L")
        
        write_string(f, "model.layers")
        write_uint32_value(f, num_layers)
        
        write_string(f, "model.vocab_size")
        write_uint32_value(f, 32000)
        
        # Embedding
        write_string(f, "model.embed_tokens.weight")
        f.write(struct.pack('<I', 2))
        f.write(struct.pack('<Q', 32000))
        f.write(struct.pack('<Q', 4096))
        f.write(struct.pack('<I', 0))
        f.write(struct.pack('<Q', 0))
        
        offset = 32000 * 4096 * 4
        
        # Layers
        for layer_idx in range(num_layers):
            # Attention
            for proj in ['q_proj', 'k_proj', 'v_proj', 'o_proj']:
                write_string(f, f"model.layers.{layer_idx}.self_attn.{proj}.weight")
                f.write(struct.pack('<I', 2))
                f.write(struct.pack('<Q', 4096))
                f.write(struct.pack('<Q', 4096))
                f.write(struct.pack('<I', 0))
                f.write(struct.pack('<Q', offset))
                offset += 4096 * 4096 * 4
            
            # FFN
            for proj in ['gate_proj', 'up_proj', 'down_proj']:
                write_string(f, f"model.layers.{layer_idx}.mlp.{proj}.weight")
                f.write(struct.pack('<I', 2))
                dim1, dim2 = (4096, 11008) if proj != 'down_proj' else (11008, 4096)
                f.write(struct.pack('<Q', dim1))
                f.write(struct.pack('<Q', dim2))
                f.write(struct.pack('<I', 0))
                f.write(struct.pack('<Q', offset))
                offset += dim1 * dim2 * 4
            
            # Norms
            for norm in ['input_layernorm', 'post_attention_layernorm']:
                write_string(f, f"model.layers.{layer_idx}.{norm}.weight")
                f.write(struct.pack('<I', 1))
                f.write(struct.pack('<Q', 4096))
                f.write(struct.pack('<I', 0))
                f.write(struct.pack('<Q', offset))
                offset += 4096 * 4
        
        # Output layer
        write_string(f, "lm_head.weight")
        f.write(struct.pack('<I', 2))
        f.write(struct.pack('<Q', 32000))
        f.write(struct.pack('<Q', 4096))
        f.write(struct.pack('<I', 0))
        f.write(struct.pack('<Q', offset))
        
        # Final norm
        write_string(f, "model.norm.weight")
        f.write(struct.pack('<I', 1))
        f.write(struct.pack('<Q', 4096))
        f.write(struct.pack('<I', 0))
        f.write(struct.pack('<Q', offset))
    
    file_size = output_path.stat().st_size
    expected_attention = num_layers * 4
    expected_ffn = num_layers * 3
    expected_embedding = 1  # embed_tokens
    expected_norm = num_layers * 2 + 1  # layer norms + final norm
    
    print(f"✅ Created large test GGUF file: {output_path}")
    print(f"   Size: {file_size:,} bytes")
    print(f"   Layers: {num_layers}")
    print(f"   Tensors: {tensor_count}")
    print(f"   Expected counts:")
    print(f"     - Attention: {expected_attention}")
    print(f"     - FFN: {expected_ffn}")
    print(f"     - Embedding: {expected_embedding}")
    print(f"     - Normalization: {expected_norm}")

if __name__ == "__main__":
    # Create test files
    test_dir = Path("D:/RawrXD-production-lazy-init/test_data/gguf")
    test_dir.mkdir(parents=True, exist_ok=True)
    
    # Small test file
    create_test_gguf(test_dir / "test_minimal.gguf")
    print()
    
    # Realistic 7B-style model (32 layers)
    create_large_test_gguf(test_dir / "test_7b_32L.gguf", num_layers=32)
    print()
    
    print("🎉 Test files generated successfully!")
    print(f"\nTest with:")
    print(f"  cd D:\\RawrXD-production-lazy-init")
    print(f"  .\\build\\src\\execai\\Release\\gguf_analyzer_masm64.exe test_data\\gguf\\test_minimal.gguf test_data\\gguf\\test_minimal.exec")
