import struct

with open(r'd:\rawrxd\models\model.gguf', 'wb') as f:
    # Magic: GGUF (LE)
    f.write(struct.pack('<I', 0x46554747))
    # Version: 1
    f.write(struct.pack('<I', 3))
    # Tensors: 0
    f.write(struct.pack('<Q', 0))
    # Metadata: 0
    f.write(struct.pack('<Q', 0))
    # Padding to be map-able
    f.write(b'\x00' * 4096)
