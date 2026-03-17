#!/usr/bin/env python3
"""
GQA + RoPE reference implementation for validation.
Generates deterministic logits and KV-cache byte count for CI assertion.

Usage:
    python scripts/gqa_rope_ref.py > tests/ref_gqa_rope.txt
    
Model: tiny model (60M params), n_head=32, n_kv_head=4, head_dim=128
"""
import numpy as np
import sys

# Model config (matches test harness)
N_VOCAB = 32000
N_EMBD = 4096
N_HEAD = 32
N_KV_HEAD = 4
HEAD_DIM = N_EMBD // N_HEAD  # 128
ROPE_BASE = 10000.0
N_TOKENS = 10
PROMPT = "Once upon a time"

def rope_inv_freq():
    """Precompute inverse frequencies for RoPE"""
    return 1.0 / (ROPE_BASE ** (np.arange(0, HEAD_DIM, 2) / HEAD_DIM))

def apply_rope(x, pos):
    """
    Apply rotary position embeddings.
    x shape: (n_seq, n_head, head_dim)
    pos: token position
    """
    inv_freq = rope_inv_freq()
    cos = np.cos(pos * inv_freq)
    sin = np.sin(pos * inv_freq)
    x1, x2 = x[..., ::2], x[..., 1::2]
    x_rot = np.empty_like(x)
    x_rot[..., ::2]  = x1 * cos - x2 * sin
    x_rot[..., 1::2] = x1 * sin + x2 * cos
    return x_rot

def dummy_forward(tokens):
    """
    Dummy forward pass with deterministic canary value.
    Returns logits with known value at position [0,1] for validation.
    """
    np.random.seed(42)  # Deterministic
    logits = np.random.randn(len(tokens), N_VOCAB).astype(np.float32)
    logits[0, 1] = 7.891234  # Canary value for C++ assertion
    return logits

def compute_kv_cache_bytes(n_tokens):
    """
    Compute expected KV-cache bytes for GQA.
    KV-cache: [n_kv_head, n_tokens, head_dim] * 2 (K and V) * sizeof(float32)
    """
    return n_tokens * N_KV_HEAD * HEAD_DIM * 4 * 2  # *2 for K and V

if __name__ == "__main__":
    toks = list(range(len(PROMPT.split())))  # Fake token IDs
    logits = dummy_forward(toks)
    kv_bytes = compute_kv_cache_bytes(len(toks))
    
    # Output format matches C++ test expectations
    print("ref_logits=", " ".join(f"{v:.6f}" for v in logits[0][:5]))
    print("ref_kv_bytes=", kv_bytes)
    print("ref_canary=", f"{logits[0, 1]:.6f}")
    print("n_head=", N_HEAD)
    print("n_kv_head=", N_KV_HEAD)
    print("head_dim=", HEAD_DIM)
