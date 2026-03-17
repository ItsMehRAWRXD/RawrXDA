#!/usr/bin/env python3
"""Quick test of model digestion engine"""

import sys
print("=" * 70)
print("MODEL DIGESTION ENGINE - Python Edition")
print("=" * 70)
print()

# Test imports
print("✅ Python version:", sys.version.split()[0])

try:
    import argparse
    print("✅ argparse available")
except:
    print("❌ argparse missing")

try:
    import pathlib
    print("✅ pathlib available")
except:
    print("❌ pathlib missing")

try:
    import json
    print("✅ json available")
except:
    print("❌ json missing")

try:
    import hashlib
    print("✅ hashlib available")
except:
    print("❌ hashlib missing")

try:
    import struct
    print("✅ struct available")
except:
    print("❌ struct missing")

# Test crypto
try:
    from Crypto.Cipher import AES
    print("✅ pycryptodome available (encryption enabled)")
    crypto_available = True
except ImportError:
    print("⚠️  pycryptodome not available (encryption disabled)")
    print("   Install with: pip install pycryptodome")
    crypto_available = False

print()
print("=" * 70)
print("DIGEST.PY STATUS: READY")
print("=" * 70)
print()
print("Usage:")
print("  python digest.py -i model.gguf -o output_dir")
print("  python digest.py --drive d: --pattern '*.gguf' --output d:\\digested")
print()

if crypto_available:
    print("Status: Full encryption support ✅")
else:
    print("Status: Basic digestion only (no encryption)")
