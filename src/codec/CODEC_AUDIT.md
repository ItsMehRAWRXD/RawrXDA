# CODEC_AUDIT.md

## Folder: `src/codec/`

### Summary
This folder contains compression and codec logic for the IDE/CLI, including routines for compressing and decompressing model files and data streams. The code is implemented in C++ and may rely on external libraries for compression (zlib, zstd, lz4). Some routines are stubs or placeholders for future MASM migration.

### Key Files Audited
- `compression.cpp`: Implements compression and decompression logic. May use C++ standard library and external compression libraries.
- `CODEC_FOLDER_AUDIT.md`: Existing audit file (to be updated with MASM migration plan).

### External Dependencies
- C++ standard library (memory, threading, chrono)
- Compression libraries (zlib, zstd, lz4)

### Stub/Placeholder Routines
- Some compression routines may be stubbed or simplified for unsupported features.
- Decompression and error handling may be incomplete or placeholder.

### MASM Migration Targets
- All compression and decompression routines to be ported to MASM (`masm/compression.asm`, `ggml_masm/compression.asm`).
- Remove all external compression library dependencies in favor of MASM64 implementations.
- Refactor codec logic for MASM64 compatibility.

### Next Steps
- [ ] Implement MASM compression and decompression routines and wire into codec logic.
- [ ] Refactor codec logic to call MASM routines for compression and decompression.
- [ ] Remove all external compression dependencies.
- [ ] Add regression and fuzz tests for MASM compression and decompression.
- [ ] Document all MASM entry points and test coverage.
