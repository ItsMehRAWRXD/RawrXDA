"""
RawrXD Polymorphic Encoder - Python Test Utilities
Generates test payloads and validates encoder transformations
"""

import os
import random
import struct
from pathlib import Path


def generate_noise_payload(size_kb: int = 64) -> bytearray:
    """
    Generate high-entropy binary payload for encoder testing.
    Injects specific markers to validate ROR/XOR chain logic.
    """
    size_bytes = size_kb * 1024
    payload = bytearray(os.urandom(size_bytes))
    
    # Inject markers every 256 bytes
    for i in range(0, size_bytes, 256):
        payload[i] = 0x42  # Classic marker byte
        
    return payload


def generate_pe_stub() -> bytearray:
    """
    Generate minimal x64 PE stub for encoder testing.
    Minimal structure: DOS header + PE header + minimal .text section
    """
    pe = bytearray()
    
    # DOS Header
    dos_header = bytearray([
        0x4D, 0x5A, 0x90, 0x00, 0x03, 0x00, 0x00, 0x00,
        0x04, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
        0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    ] + [0x00] * 32)
    
    pe.extend(dos_header)
    pe.extend([0x00] * (0x80 - len(pe)))
    
    # PE Signature (at offset 0x80)
    pe.extend(b'PE\x00\x00')
    
    # COFF File Header
    pe.extend(struct.pack('<H', 0x8664))      # Machine: x64
    pe.extend(struct.pack('<H', 1))           # NumberOfSections
    pe.extend(struct.pack('<I', 0))           # TimeDateStamp
    pe.extend(struct.pack('<I', 0))           # PointerToSymbolTable
    pe.extend(struct.pack('<I', 0))           # NumberOfSymbols
    pe.extend(struct.pack('<H', 240))         # SizeOfOptionalHeader
    pe.extend(struct.pack('<H', 0x0022))      # Characteristics
    
    # Optional Header (PE32+)
    pe.extend(struct.pack('<H', 0x020B))      # Magic
    pe.extend(struct.pack('<BB', 2, 0x22))    # Linker version
    pe.extend(struct.pack('<I', 0x1000))      # SizeOfCode
    pe.extend(struct.pack('<I', 0))           # SizeOfInitData
    pe.extend(struct.pack('<I', 0))           # SizeOfUninitData
    pe.extend(struct.pack('<I', 0x1000))      # AddressOfEntryPoint
    pe.extend(struct.pack('<I', 0x1000))      # BaseOfCode
    pe.extend(struct.pack('<Q', 0x140000000)) # ImageBase
    
    pe.extend(struct.pack('<I', 0x1000))      # SectionAlignment
    pe.extend(struct.pack('<I', 0x200))       # FileAlignment
    pe.extend(struct.pack('<HH', 6, 0))       # OS version
    pe.extend(struct.pack('<HH', 0, 0))       # Image version
    pe.extend(struct.pack('<HH', 6, 0))       # Subsystem version
    pe.extend(struct.pack('<I', 0))           # Win32VersionValue
    pe.extend(struct.pack('<I', 0x2000))      # SizeOfImage
    pe.extend(struct.pack('<I', 0x400))       # SizeOfHeaders
    pe.extend(struct.pack('<I', 0))           # CheckSum
    pe.extend(struct.pack('<H', 3))           # Subsystem: CUI
    pe.extend(struct.pack('<H', 0))           # DllCharacteristics
    
    # Stack/Heap sizes
    pe.extend(struct.pack('<Q', 0x100000))    # SizeOfStackReserve
    pe.extend(struct.pack('<Q', 0x1000))      # SizeOfStackCommit
    pe.extend(struct.pack('<Q', 0x100000))    # SizeOfHeapReserve
    pe.extend(struct.pack('<Q', 0x1000))      # SizeOfHeapCommit
    pe.extend(struct.pack('<I', 0))           # LoaderFlags
    pe.extend(struct.pack('<I', 16))          # NumberOfRvaAndSizes
    
    # Data Directories (16 entries)
    for _ in range(16):
        pe.extend(struct.pack('<II', 0, 0))
    
    # Section Header (.text)
    pe.extend(b'.text\x00\x00\x00')
    pe.extend(struct.pack('<I', 0x1000))      # VirtualSize
    pe.extend(struct.pack('<I', 0x1000))      # VirtualAddress
    pe.extend(struct.pack('<I', 0x200))       # SizeOfRawData
    pe.extend(struct.pack('<I', 0x400))       # PointerToRawData
    pe.extend(struct.pack('<I', 0))           # PointerToRelocations
    pe.extend(struct.pack('<I', 0))           # PointerToLinenumbers
    pe.extend(struct.pack('<H', 0))           # NumberOfRelocations
    pe.extend(struct.pack('<H', 0))           # NumberOfLinenumbers
    pe.extend(struct.pack('<I', 0x60000020))  # Characteristics
    
    # Pad headers to 0x400
    pe.extend([0x00] * (0x400 - len(pe)))
    
    # Add minimal code section
    pe.extend([0x90] * 0x200)  # NOP sled
    
    return pe


def create_test_keys(count: int = 5) -> list:
    """
    Generate test encryption keys of various lengths.
    """
    keys = []
    for i in range(count):
        key_len = random.choice([16, 32, 64, 128, 256])
        key = bytearray(os.urandom(key_len))
        keys.append((f"test_key_{i}", key))
    return keys


def create_project_structure():
    """
    Set up directory structure for Polymorphic Engine project.
    """
    dirs = [
        'payloads',
        'payloads/test_vectors',
        'payloads/pe_stubs',
        'output',
        'output/encoded',
        'output/decoded',
    ]
    
    for d in dirs:
        path = Path(d)
        path.mkdir(parents=True, exist_ok=True)
        print(f"✓ Created: {d}")


def generate_test_suite():
    """
    Generate complete test suite for encoder validation.
    """
    print("[*] Generating RawrXD Encoder Test Suite\n")
    
    create_project_structure()
    
    # 1. Generate random payloads
    print("[+] Generating test payloads...")
    for size_kb in [1, 4, 16, 64, 128]:
        payload = generate_noise_payload(size_kb)
        filename = f"payloads/test_vectors/payload_{size_kb}kb.bin"
        with open(filename, "wb") as f:
            f.write(payload)
        print(f"    ✓ {filename} ({len(payload)} bytes)")
    
    # 2. Generate PE stubs
    print("\n[+] Generating PE stubs...")
    pe_stub = generate_pe_stub()
    with open("payloads/pe_stubs/minimal_x64.exe", "wb") as f:
        f.write(pe_stub)
    print(f"    ✓ payloads/pe_stubs/minimal_x64.exe ({len(pe_stub)} bytes)")
    
    # 3. Generate test keys
    print("\n[+] Generating encryption keys...")
    keys = create_test_keys()
    for key_name, key_data in keys:
        filename = f"payloads/test_vectors/{key_name}.key"
        with open(filename, "wb") as f:
            f.write(key_data)
        print(f"    ✓ {filename} ({len(key_data)} bytes)")
    
    # 4. Create algorithm reference
    print("\n[+] Creating algorithm reference...")
    algo_ref = """# Polymorphic Encoder Algorithm Reference

## Supported Algorithms

| ID | Name | Type | Symmetric | Notes |
|----|------|------|-----------|-------|
| 0x01 | XOR Chain | Substitution | Yes | Chained XOR with key cycling |
| 0x02 | Rolling XOR | Stream | Yes | Position-dependent XOR transform |
| 0x03 | RC4 Stream | Stream | Yes | Custom S-box stream cipher |
| 0x04 | AES-NI CTR | Block | Yes | Hardware-accelerated AES counter |
| 0x05 | Polymorphic | Dynamic | No | Runtime self-modifying stub |
| 0x06 | AVX-512 Mix | SIMD | Yes | Vectorized bitwise mixing |

## Test Vectors

- Small (1 KB): Quick validation
- Medium (16 KB): Standard tests
- Large (128 KB): Performance benchmarks

## Usage

```cpp
RawrXD::PolymorphicEncoder encoder;
encoder.SetAlgorithm(ENC_XOR_CHAIN);
encoder.SetKey(key_data, key_len);
encoder.Encode(input, output, size);
```
"""
    with open("payloads/ALGORITHMS.md", "w") as f:
        f.write(algo_ref)
    print("    ✓ payloads/ALGORITHMS.md")
    
    print("\n[✓] Test suite generation complete!")
    print(f"    Location: {os.path.abspath('payloads')}")


if __name__ == "__main__":
    generate_test_suite()
