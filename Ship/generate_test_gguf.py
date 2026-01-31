# 🧪 Test GGUF File Generator

## Generate minimal GGUF files for testing without downloading 7B+ models

import struct
import numpy as np
import os
from pathlib import Path

class GGUFType:
    """GGUF metadata value types"""
    UINT8 = 0
    INT8 = 1
    UINT16 = 2
    INT16 = 3
    UINT32 = 4
    INT32 = 5
    FLOAT32 = 6
    BOOL = 7
    STRING = 8
    ARRAY = 9
    UINT64 = 10
    INT64 = 11
    FLOAT64 = 12

class GGMLType:
    """GGML quantization types"""
    F32 = 0
    F16 = 1
    Q4_0 = 2
    Q4_1 = 3
    Q5_0 = 6
    Q5_1 = 7
    Q8_0 = 8
    Q8_1 = 9
    Q2_K = 10
    Q4_K = 12
    Q5_K = 13
    Q6_K = 14

class GGUFWriter:
    """Minimal GGUF file generator for testing"""
    
    def __init__(self):
        self.magic = 0x46554747  # "GGUF"
        self.version = 3
        self.tensor_count = 0
        self.metadata = []
        self.tensors = []
        self.alignment = 32
        
    def add_architecture(self, arch: str):
        """Add general.architecture metadata"""
        self.add_string("general.architecture", arch)
        
    def add_string(self, key: str, value: str):
        """Add string metadata"""
        self.metadata.append({
            'key': key,
            'type': GGUFType.STRING,
            'value': value
        })
        
    def add_uint32(self, key: str, value: int):
        """Add uint32 metadata"""
        self.metadata.append({
            'key': key,
            'type': GGUFType.UINT32,
            'value': value
        })
        
    def add_float32(self, key: str, value: float):
        """Add float32 metadata"""
        self.metadata.append({
            'key': key,
            'type': GGUFType.FLOAT32,
            'value': value
        })
        
    def add_tensor(self, name: str, shape: tuple, ggml_type: int, data=None):
        """Add tensor definition"""
        n_dims = len(shape)
        n_elements = np.prod(shape)
        
        if data is None:
            # Generate random data based on type
            if ggml_type == GGMLType.F32:
                data = np.random.randn(n_elements).astype(np.float32)
            elif ggml_type == GGMLType.Q4_0:
                # Q4_0: 32 elements per 18-byte block
                n_blocks = (n_elements + 31) // 32
                data = bytearray(n_blocks * 18)
                # Fill with random nibbles
                for i in range(len(data)):
                    data[i] = np.random.randint(0, 256)
            elif ggml_type == GGMLType.Q2_K:
                # Q2_K: 256 elements per 84-byte block
                n_blocks = (n_elements + 255) // 256
                data = bytearray(n_blocks * 84)
                for i in range(len(data)):
                    data[i] = np.random.randint(0, 256)
            else:
                raise ValueError(f"Unsupported type: {ggml_type}")
        
        self.tensors.append({
            'name': name,
            'shape': shape,
            'n_dims': n_dims,
            'ggml_type': ggml_type,
            'data': data
        })
        self.tensor_count += 1
        
    def write(self, filename: str):
        """Write GGUF file to disk"""
        with open(filename, 'wb') as f:
            # Header
            f.write(struct.pack('<I', self.magic))      # magic
            f.write(struct.pack('<I', self.version))    # version
            f.write(struct.pack('<Q', self.tensor_count))  # tensor_count
            f.write(struct.pack('<Q', len(self.metadata))) # kv_count
            
            # Metadata key-value pairs
            for kv in self.metadata:
                # Write key
                key_bytes = kv['key'].encode('utf-8')
                f.write(struct.pack('<I', len(key_bytes)))
                f.write(key_bytes)
                
                # Write type
                f.write(struct.pack('<I', kv['type']))
                
                # Write value
                if kv['type'] == GGUFType.STRING:
                    val_bytes = kv['value'].encode('utf-8')
                    f.write(struct.pack('<Q', len(val_bytes)))
                    f.write(val_bytes)
                elif kv['type'] == GGUFType.UINT32:
                    f.write(struct.pack('<I', kv['value']))
                elif kv['type'] == GGUFType.FLOAT32:
                    f.write(struct.pack('<f', kv['value']))
                # Add more types as needed
            
            # Tensor info array
            current_offset = 0
            for tensor in self.tensors:
                # Tensor name
                name_bytes = tensor['name'].encode('utf-8')
                f.write(struct.pack('<I', len(name_bytes)))
                f.write(name_bytes)
                
                # Dimensions
                f.write(struct.pack('<I', tensor['n_dims']))
                for dim in tensor['shape']:
                    f.write(struct.pack('<Q', dim))
                for _ in range(4 - tensor['n_dims']):
                    f.write(struct.pack('<Q', 1))  # Pad to 4 dims
                
                # Type and offset
                f.write(struct.pack('<I', tensor['ggml_type']))
                f.write(struct.pack('<Q', current_offset))
                
                # Update offset
                if tensor['ggml_type'] == GGMLType.F32:
                    data_size = len(tensor['data']) * 4
                elif isinstance(tensor['data'], (bytes, bytearray)):
                    data_size = len(tensor['data'])
                else:
                    data_size = tensor['data'].nbytes
                
                current_offset += data_size
            
            # Align to boundary
            padding = (self.alignment - (f.tell() % self.alignment)) % self.alignment
            f.write(b'\x00' * padding)
            
            # Tensor data
            for tensor in self.tensors:
                if isinstance(tensor['data'], np.ndarray):
                    f.write(tensor['data'].tobytes())
                else:
                    f.write(bytes(tensor['data']))
        
        print(f"✅ Created GGUF: {filename} ({os.path.getsize(filename)} bytes)")

def create_tiny_test_model():
    """Create minimal 1M parameter test model"""
    writer = GGUFWriter()
    
    # Architecture metadata (mimics TinyLlama)
    writer.add_architecture("llama")
    writer.add_string("general.name", "TestModel-1M")
    writer.add_uint32("llama.vocab_size", 512)
    writer.add_uint32("llama.context_length", 512)
    writer.add_uint32("llama.embedding_length", 64)
    writer.add_uint32("llama.block_count", 2)  # Just 2 layers
    writer.add_uint32("llama.feed_forward_length", 256)
    writer.add_uint32("llama.attention.head_count", 4)
    writer.add_uint32("llama.attention.head_count_kv", 4)
    writer.add_float32("llama.attention.layer_norm_rms_epsilon", 1e-5)
    writer.add_float32("llama.rope.freq_base", 10000.0)
    
    # Tokenizer metadata
    writer.add_string("tokenizer.ggml.model", "gpt2")
    writer.add_uint32("tokenizer.ggml.bos_token_id", 1)
    writer.add_uint32("tokenizer.ggml.eos_token_id", 2)
    
    # Token embeddings: [512 vocab, 64 dims] = 32K params
    writer.add_tensor("token_embd.weight", (64, 512), GGMLType.F32)
    
    # Output norm: [64] = 64 params
    writer.add_tensor("output_norm.weight", (64,), GGMLType.F32)
    
    # For 2 layers (each ~250K params):
    for layer in range(2):
        # Attention norm: [64]
        writer.add_tensor(f"blk.{layer}.attn_norm.weight", (64,), GGMLType.F32)
        
        # Q,K,V,O projections: [64, 64] each = 4K params each
        writer.add_tensor(f"blk.{layer}.attn_q.weight", (64, 64), GGMLType.Q4_0)
        writer.add_tensor(f"blk.{layer}.attn_k.weight", (64, 64), GGMLType.Q4_0)
        writer.add_tensor(f"blk.{layer}.attn_v.weight", (64, 64), GGMLType.Q4_0)
        writer.add_tensor(f"blk.{layer}.attn_output.weight", (64, 64), GGMLType.Q4_0)
        
        # FFN norm: [64]
        writer.add_tensor(f"blk.{layer}.ffn_norm.weight", (64,), GGMLType.F32)
        
        # FFN projections: [256, 64], [64, 256], [256, 64] = 49K params total
        writer.add_tensor(f"blk.{layer}.ffn_gate.weight", (256, 64), GGMLType.Q4_0)
        writer.add_tensor(f"blk.{layer}.ffn_down.weight", (64, 256), GGMLType.Q4_0)
        writer.add_tensor(f"blk.{layer}.ffn_up.weight", (256, 64), GGMLType.Q4_0)
    
    writer.write("test_model_1m.gguf")
    return "test_model_1m.gguf"

def create_q2k_test_model():
    """Create Q2_K quantized test model (extreme compression)"""
    writer = GGUFWriter()
    
    writer.add_architecture("llama")
    writer.add_string("general.name", "TestModel-Q2K")
    writer.add_uint32("llama.vocab_size", 1024)
    writer.add_uint32("llama.context_length", 1024)
    writer.add_uint32("llama.embedding_length", 128)
    writer.add_uint32("llama.block_count", 4)
    writer.add_uint32("llama.feed_forward_length", 512)
    writer.add_uint32("llama.attention.head_count", 8)
    writer.add_uint32("llama.attention.head_count_kv", 8)
    
    # Embeddings: F32 (high precision needed)
    writer.add_tensor("token_embd.weight", (128, 1024), GGMLType.F32)
    writer.add_tensor("output_norm.weight", (128,), GGMLType.F32)
    
    # All weight matrices in Q2_K (2-bit quantization!)
    for layer in range(4):
        writer.add_tensor(f"blk.{layer}.attn_norm.weight", (128,), GGMLType.F32)
        writer.add_tensor(f"blk.{layer}.attn_q.weight", (128, 128), GGMLType.Q2_K)
        writer.add_tensor(f"blk.{layer}.attn_k.weight", (128, 128), GGMLType.Q2_K)
        writer.add_tensor(f"blk.{layer}.attn_v.weight", (128, 128), GGMLType.Q2_K)
        writer.add_tensor(f"blk.{layer}.attn_output.weight", (128, 128), GGMLType.Q2_K)
        writer.add_tensor(f"blk.{layer}.ffn_norm.weight", (128,), GGMLType.F32)
        writer.add_tensor(f"blk.{layer}.ffn_gate.weight", (512, 128), GGMLType.Q2_K)
        writer.add_tensor(f"blk.{layer}.ffn_down.weight", (128, 512), GGMLType.Q2_K)
        writer.add_tensor(f"blk.{layer}.ffn_up.weight", (512, 128), GGMLType.Q2_K)
    
    writer.write("test_model_q2k.gguf")
    return "test_model_q2k.gguf"

def create_malformed_test():
    """Create intentionally malformed GGUF for error handling tests"""
    files = []
    
    # Test 1: Invalid magic
    with open("test_invalid_magic.gguf", 'wb') as f:
        f.write(struct.pack('<I', 0xDEADBEEF))  # Wrong magic
        f.write(struct.pack('<I', 3))
        f.write(struct.pack('<Q', 0))
        f.write(struct.pack('<Q', 0))
    files.append("test_invalid_magic.gguf")
    
    # Test 2: Unsupported version
    with open("test_bad_version.gguf", 'wb') as f:
        f.write(struct.pack('<I', 0x46554747))
        f.write(struct.pack('<I', 99))  # Version 99
        f.write(struct.pack('<Q', 0))
        f.write(struct.pack('<Q', 0))
    files.append("test_bad_version.gguf")
    
    # Test 3: Truncated file
    with open("test_truncated.gguf", 'wb') as f:
        f.write(struct.pack('<I', 0x46554747))
        f.write(struct.pack('<I', 3))
        # Missing rest of header
    files.append("test_truncated.gguf")
    
    print(f"✅ Created {len(files)} malformed test files")
    return files

def validate_generated_files():
    """Read and validate generated GGUF files"""
    test_files = [
        "test_model_1m.gguf",
        "test_model_q2k.gguf"
    ]
    
    for filename in test_files:
        if not os.path.exists(filename):
            continue
            
        with open(filename, 'rb') as f:
            # Read header
            magic = struct.unpack('<I', f.read(4))[0]
            version = struct.unpack('<I', f.read(4))[0]
            n_tensors = struct.unpack('<Q', f.read(8))[0]
            n_kv = struct.unpack('<Q', f.read(8))[0]
            
            print(f"\n📄 {filename}:")
            print(f"  Magic: 0x{magic:08X} {'✅' if magic == 0x46554747 else '❌'}")
            print(f"  Version: {version}")
            print(f"  Tensors: {n_tensors}")
            print(f"  Metadata KV: {n_kv}")
            print(f"  Size: {os.path.getsize(filename):,} bytes")

if __name__ == "__main__":
    print("🧪 GGUF Test File Generator\n")
    
    # Create test models
    print("Creating tiny 1M parameter model...")
    create_tiny_test_model()
    
    print("\nCreating Q2_K compressed model...")
    create_q2k_test_model()
    
    print("\nCreating malformed test files...")
    create_malformed_test()
    
    print("\n" + "="*60)
    print("Validating generated files...")
    validate_generated_files()
    
    print("\n" + "="*60)
    print("✅ Test file generation complete!")
    print("\nUsage in PowerShell:")
    print('  $dll = [System.Reflection.Assembly]::LoadFile("$PWD\\RawrXD_NativeModelBridge.dll")')
    print('  # Test loading')
    print('  $method = $dll.GetType("NativeBridge").GetMethod("LoadModelNative")')
    print('  $ctx = $method.Invoke($null, @("test_model_1m.gguf", [ref]$null))')
