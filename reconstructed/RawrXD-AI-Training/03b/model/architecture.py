#!/usr/bin/env python3
"""Phase 3b: Architecture verification — runs model_architecture.py smoke test"""
import sys
sys.path.insert(0, "F:/RawrXD-AI-Training")
from model_architecture import RawrXDTransformer, RawrXDConfig
import torch

if __name__ == "__main__":
    print("=" * 60)
    print("RawrXD Model Architecture Verification")
    print("=" * 60)
    
    for name, factory in [("nano", RawrXDConfig.nano), ("small", RawrXDConfig.small),
                           ("medium", RawrXDConfig.medium), ("large", RawrXDConfig.large)]:
        config = factory()
        model = RawrXDTransformer(config)
        n = sum(p.numel() for p in model.parameters())
        print(f"  RawrXD-{name:7s}: {n/1e6:>8.1f}M params  ({config.n_layers}L/{config.n_heads}H/{config.dim}D)")
    
    # Smoke test forward pass
    print("\n[Test] Forward pass with RawrXD-Nano...")
    config = RawrXDConfig.nano()
    model = RawrXDTransformer(config)
    dummy = torch.randint(0, config.vocab_size, (2, 128))
    out = model(dummy)
    print(f"  Input:  {dummy.shape}")
    print(f"  Output: {out.shape}")
    print(f"  Loss check: {torch.nn.functional.cross_entropy(out.view(-1, config.vocab_size), dummy.view(-1)).item():.4f}")
    print("\n[OK] Architecture verified.")
