#!/usr/bin/env python3
"""Phase 4: Export trained model to GGUF format (F16, Q8_0, Q4_0)"""
import struct, json, argparse, sys, os
import numpy as np
from pathlib import Path
import torch

sys.path.insert(0, str(Path(__file__).parent))
from model_architecture import RawrXDTransformer, RawrXDConfig

GGUF_MAGIC = b"GGUF"
GGUF_VERSION = 3

# GGUF value types
GGUF_TYPE_UINT8   = 0
GGUF_TYPE_INT8    = 1
GGUF_TYPE_UINT16  = 2
GGUF_TYPE_INT16   = 3
GGUF_TYPE_UINT32  = 4
GGUF_TYPE_INT32   = 5
GGUF_TYPE_FLOAT32 = 6
GGUF_TYPE_BOOL    = 7
GGUF_TYPE_STRING  = 8
GGUF_TYPE_ARRAY   = 9
GGUF_TYPE_UINT64  = 10
GGUF_TYPE_INT64   = 11
GGUF_TYPE_FLOAT64 = 12

# GGML tensor types
GGML_TYPE_F32  = 0
GGML_TYPE_F16  = 1
GGML_TYPE_Q4_0 = 2
GGML_TYPE_Q8_0 = 8
GGML_TYPE_Q2_K = 10

def write_string(f, s):
    encoded = s.encode('utf-8')
    f.write(struct.pack('<Q', len(encoded)))
    f.write(encoded)

def write_kv_string(f, key, val):
    write_string(f, key)
    f.write(struct.pack('<I', GGUF_TYPE_STRING))
    write_string(f, val)

def write_kv_uint32(f, key, val):
    write_string(f, key)
    f.write(struct.pack('<I', GGUF_TYPE_UINT32))
    f.write(struct.pack('<I', val))

def write_kv_float32(f, key, val):
    write_string(f, key)
    f.write(struct.pack('<I', GGUF_TYPE_FLOAT32))
    f.write(struct.pack('<f', val))

def quantize_q8_0(data):
    """Block-wise Q8_0 quantization (32 elements per block, matching llama.cpp)"""
    flat = data.astype(np.float32).flatten()
    block_size = 32
    pad_len = (block_size - len(flat) % block_size) % block_size
    if pad_len:
        flat = np.concatenate([flat, np.zeros(pad_len, dtype=np.float32)])
    
    n_blocks = len(flat) // block_size
    result = bytearray()
    
    for i in range(n_blocks):
        block = flat[i*block_size:(i+1)*block_size]
        amax = np.max(np.abs(block))
        scale = amax / 127.0 if amax > 0 else 1.0
        q = np.clip(np.round(block / scale), -128, 127).astype(np.int8)
        # Q8_0 format: f16 scale + 32 int8 values
        result.extend(struct.pack('<e', np.float16(scale)))
        result.extend(q.tobytes())
    
    return bytes(result)

def quantize_q4_0(data):
    """Block-wise Q4_0 quantization (32 elements per block)"""
    flat = data.astype(np.float32).flatten()
    block_size = 32
    pad_len = (block_size - len(flat) % block_size) % block_size
    if pad_len:
        flat = np.concatenate([flat, np.zeros(pad_len, dtype=np.float32)])
    
    n_blocks = len(flat) // block_size
    result = bytearray()
    
    for i in range(n_blocks):
        block = flat[i*block_size:(i+1)*block_size]
        amax = np.max(np.abs(block))
        scale = amax / 7.0 if amax > 0 else 1.0
        q = np.clip(np.round(block / scale), -8, 7).astype(np.int8)
        q_unsigned = (q + 8).astype(np.uint8)
        
        # Q4_0 format: f16 scale + 16 bytes (32 nibbles packed)
        result.extend(struct.pack('<e', np.float16(scale)))
        for j in range(0, 32, 2):
            lo = q_unsigned[j] & 0x0F
            hi = q_unsigned[j+1] & 0x0F
            result.append(lo | (hi << 4))
    
    return bytes(result)

def quantize_q2_k(data):
    """K-quant Q2_K quantization (256-element superblocks).
    
    Each superblock: 256 elements → 16 sub-blocks of 16 elements each.
    Per sub-block: 2-bit weights (16 elems → 4 bytes) + 4-bit scale + 4-bit min.
    Per superblock: f16 global_scale + f16 global_min + 16×(4 bytes quants + 1 byte scale/min)
    Total: 2 + 2 + 16×(4+1) = 84 bytes for 256 elements → 2.625 bits/weight.
    
    This matches the ggml Q2_K layout used by llama.cpp.
    """
    flat = data.astype(np.float32).flatten()
    block_size = 256  # superblock size
    sub_block_size = 16
    n_sub_blocks = block_size // sub_block_size  # 16
    
    pad_len = (block_size - len(flat) % block_size) % block_size
    if pad_len:
        flat = np.concatenate([flat, np.zeros(pad_len, dtype=np.float32)])
    
    n_blocks = len(flat) // block_size
    result = bytearray()
    
    for i in range(n_blocks):
        sb = flat[i * block_size:(i + 1) * block_size]
        
        # Compute per-sub-block scales and mins
        sub_scales = np.zeros(n_sub_blocks, dtype=np.float32)
        sub_mins = np.zeros(n_sub_blocks, dtype=np.float32)
        sub_quants = np.zeros(block_size, dtype=np.uint8)
        
        for j in range(n_sub_blocks):
            sub = sb[j * sub_block_size:(j + 1) * sub_block_size]
            smin = np.min(sub)
            smax = np.max(sub)
            
            # Map [smin, smax] → [0, 3] (2-bit unsigned)
            d = smax - smin
            if d < 1e-10:
                sub_scales[j] = 0.0
                sub_mins[j] = smin
                # All zeros
            else:
                sub_scales[j] = d / 3.0
                sub_mins[j] = smin
                q = np.clip(np.round((sub - smin) / (d / 3.0)), 0, 3).astype(np.uint8)
                sub_quants[j * sub_block_size:(j + 1) * sub_block_size] = q
        
        # Global scale and min for the sub-block scales/mins
        g_scale_max = np.max(np.abs(sub_scales)) if np.max(np.abs(sub_scales)) > 0 else 1.0
        g_min_max = np.max(np.abs(sub_mins)) if np.max(np.abs(sub_mins)) > 0 else 1.0
        global_scale = g_scale_max / 15.0  # quantize sub_scales to 4 bits [0,15]
        global_min = g_min_max / 15.0      # quantize sub_mins to 4 bits [0,15]
        
        # Quantize sub-block scales and mins to 4-bit each (packed into 1 byte per sub-block)
        qs_quant = np.zeros(n_sub_blocks, dtype=np.uint8)
        qm_quant = np.zeros(n_sub_blocks, dtype=np.uint8)
        for j in range(n_sub_blocks):
            if global_scale > 1e-15:
                qs_quant[j] = min(15, int(round(abs(sub_scales[j]) / global_scale)))
            if global_min > 1e-15:
                qm_quant[j] = min(15, int(round(abs(sub_mins[j]) / global_min)))
        
        # Write superblock:
        # f16 global_scale (2 bytes)
        result.extend(struct.pack('<e', np.float16(global_scale)))
        # f16 global_min (2 bytes)
        result.extend(struct.pack('<e', np.float16(global_min)))
        
        # 16 sub-block scale/min pairs packed: lo nibble=scale, hi nibble=min (16 bytes)
        for j in range(0, n_sub_blocks, 2):
            byte_sm_0 = (qs_quant[j] & 0x0F) | ((qm_quant[j] & 0x0F) << 4)
            byte_sm_1 = (qs_quant[j+1] & 0x0F) | ((qm_quant[j+1] & 0x0F) << 4)
            result.append(byte_sm_0)
            result.append(byte_sm_1)
        
        # 256 2-bit quants packed into 64 bytes (4 per byte)
        for j in range(0, block_size, 4):
            b = (sub_quants[j] & 0x03) \
              | ((sub_quants[j+1] & 0x03) << 2) \
              | ((sub_quants[j+2] & 0x03) << 4) \
              | ((sub_quants[j+3] & 0x03) << 6)
            result.append(b)
    
    return bytes(result)

def get_ggml_type(quant_str):
    return {'f32': GGML_TYPE_F32, 'f16': GGML_TYPE_F16,
            'q8_0': GGML_TYPE_Q8_0, 'q4_0': GGML_TYPE_Q4_0,
            'q2_k': GGML_TYPE_Q2_K}[quant_str]

def tensor_to_bytes(data, quant):
    if quant == 'f32':
        return data.astype(np.float32).tobytes()
    elif quant == 'f16':
        return data.astype(np.float16).tobytes()
    elif quant == 'q8_0':
        return quantize_q8_0(data)
    elif quant == 'q4_0':
        return quantize_q4_0(data)
    elif quant == 'q2_k':
        return quantize_q2_k(data)

def export_gguf(model_path, preset, quant='f16'):
    print(f"[Export] {preset} model -> {quant} GGUF")
    
    config_map = {
        'nano': RawrXDConfig.nano(),
        'small': RawrXDConfig.small(),
        'medium': RawrXDConfig.medium(),
        'large': RawrXDConfig.large()
    }
    config = config_map[preset]
    
    # Try loading tokenizer to get actual vocab size
    tok_config_path = Path("F:/RawrXD-AI-Training/tokenizer/config.json")
    if tok_config_path.exists():
        with open(tok_config_path) as f:
            tok_cfg = json.load(f)
            config.vocab_size = tok_cfg.get('vocab_size', config.vocab_size)
    
    model = RawrXDTransformer(config)
    
    print(f"[Load] Loading checkpoint: {model_path}")
    state_dict = torch.load(model_path, map_location='cpu', weights_only=True)
    model.load_state_dict(state_dict)
    model.eval()
    
    output_path = Path(f"F:/RawrXD-AI-Training/output/RawrXD-{preset}-{quant}.gguf")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    # Collect tensor info
    tensor_list = []
    for name, param in model.named_parameters():
        if 'freqs_cis' in name:
            continue
        data = param.detach().cpu().numpy()
        # Keep norms and embeddings at higher precision
        t_quant = quant
        if 'norm' in name or 'embed' in name:
            t_quant = 'f32'
        raw = tensor_to_bytes(data, t_quant)
        ggml_type = get_ggml_type(t_quant)
        tensor_list.append((name, data.shape, ggml_type, raw))
    
    n_tensors = len(tensor_list)
    n_kv = 10  # metadata count
    
    print(f"[Write] {n_tensors} tensors, {n_kv} metadata entries")
    
    with open(output_path, 'wb') as f:
        # ─── Header ───
        f.write(GGUF_MAGIC)
        f.write(struct.pack('<I', GGUF_VERSION))
        f.write(struct.pack('<Q', n_tensors))
        f.write(struct.pack('<Q', n_kv))
        
        # ─── Metadata KV ───
        write_kv_string(f, 'general.architecture', 'llama')
        write_kv_string(f, 'general.name', f'RawrXD-{preset}')
        write_kv_uint32(f, 'llama.block_count', config.n_layers)
        write_kv_uint32(f, 'llama.context_length', config.max_seq_len)
        write_kv_uint32(f, 'llama.embedding_length', config.dim)
        write_kv_uint32(f, 'llama.feed_forward_length', int(config.dim * 3.5))
        write_kv_uint32(f, 'llama.attention.head_count', config.n_heads)
        write_kv_uint32(f, 'llama.attention.head_count_kv', config.n_kv_heads)
        write_kv_float32(f, 'llama.rope.freq_base', config.rope_theta)
        write_kv_string(f, 'tokenizer.ggml.model', 'gpt2')
        
        # ─── Tensor Info ───
        data_blobs = []
        offset = 0
        for name, shape, ggml_type, raw in tensor_list:
            write_string(f, name)
            f.write(struct.pack('<I', len(shape)))        # n_dims
            for d in shape:
                f.write(struct.pack('<Q', d))
            f.write(struct.pack('<I', ggml_type))         # type
            f.write(struct.pack('<Q', offset))            # offset into data section
            
            data_blobs.append(raw)
            blob_len = len(raw)
            padding = (32 - (blob_len % 32)) % 32
            offset += blob_len + padding
        
        # ─── Alignment padding before data ───
        current = f.tell()
        pad = (32 - (current % 32)) % 32
        f.write(b'\x00' * pad)
        
        # ─── Tensor Data ───
        for raw in data_blobs:
            f.write(raw)
            padding = (32 - (len(raw) % 32)) % 32
            f.write(b'\x00' * padding)
    
    size_gb = output_path.stat().st_size / (1024**3)
    size_mb = output_path.stat().st_size / (1024**2)
    if size_gb >= 1.0:
        print(f"[Done] {output_path} ({size_gb:.2f} GB)")
    else:
        print(f"[Done] {output_path} ({size_mb:.1f} MB)")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="RawrXD GGUF Export")
    parser.add_argument("--preset", default="small", choices=["nano", "small", "medium", "large"])
    parser.add_argument("--quant", default="q8_0", choices=['f32', 'f16', 'q8_0', 'q4_0', 'q2_k'])
    parser.add_argument("--model_path", required=True, help="Path to trained .pt checkpoint")
    args = parser.parse_args()
    
    export_gguf(args.model_path, args.preset, args.quant)
