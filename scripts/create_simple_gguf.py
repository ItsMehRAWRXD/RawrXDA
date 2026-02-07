#!/usr/bin/env python3
# Create a minimal GGUF-compatible test file for our GGUFLoader parser
# Structure (simplified):
# - magic (4 bytes) 'GGUF'
# - version (uint32)
# - tensorCount (uint64)
# - metadataSize (uint64)
# - For each tensor:
#   - nameLen (uint32), name (bytes), offset (uint64)
# - Data regions at offsets: packedSz (uint32) + payload (bytes)
import struct
import os

outfile = os.path.join(os.path.dirname(__file__), '..', 'build', 'tests', 'simple_test_model.gguf')
if not os.path.exists(os.path.dirname(outfile)):
    os.makedirs(os.path.dirname(outfile), exist_ok=True)

# Payload: some floats (4 bytes each)
floats = [0.1, -0.2, 0.3, -0.4, 1.0]
payload = b''.join(struct.pack('<f', f) for f in floats)
packedSz = len(payload)

# We'll create a single tensor
name = b'token_embed.weight'
nameLen = len(name)

# Header
magic = b'GGUF'
version = 1
tensorCount = 1
metadataSize = 0

offset_after_directory = 4 + 4 + 8 + 8 + (4 + nameLen + 8)  # header + one directory entry
# Align payload offset to 16 bytes for safety
payload_offset = (offset_after_directory + 15) & ~15

with open(outfile, 'wb') as f:
    f.write(magic)
    f.write(struct.pack('<I', version))
    f.write(struct.pack('<Q', tensorCount))
    f.write(struct.pack('<Q', metadataSize))
    # Directory entry
    f.write(struct.pack('<I', nameLen))
    f.write(name)
    f.write(struct.pack('<Q', payload_offset))
    # Pad until payload_offset
    cur = f.tell()
    if cur < payload_offset:
        f.write(b'\x00' * (payload_offset - cur))
    # Write payload size then payload
    f.write(struct.pack('<I', packedSz))
    f.write(payload)

print('Wrote', outfile)