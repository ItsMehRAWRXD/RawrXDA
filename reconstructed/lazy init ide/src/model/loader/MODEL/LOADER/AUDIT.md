# MODEL_LOADER_AUDIT.md

## Folder: `src/model_loader/`

### Summary
This folder contains all model loading logic for the IDE/CLI, supporting GGUF, HuggingFace, Ollama, and MASM-compressed formats. The code is implemented in C++/Qt and uses a modular loader architecture with enhanced routing, streaming, and server support. Some routines are still placeholders for future decompression and remote model support.

### Key Files Audited
- `model_loader.cpp`, `ModelLoader.cpp`: Main loader classes, support for GGUF, streaming, and server integration. Modular, but some logic is duplicated between files.
- `enhanced_model_loader.cpp`: Format router, decompression, HuggingFace/Ollama proxy, and GGUF server integration. Handles temp files and error reporting. Decompression routines are currently placeholders.

### External Dependencies
- Qt (signals, slots, QDebug, QStandardPaths)
- std::filesystem, std::chrono, std::unique_ptr
- GGUF server, InferenceEngine, FormatRouter, HFDownloader, OllamaProxy
- Compression libraries (planned: zstd, gzip, lz4)

### Stub/Placeholder Routines
- Decompression for GZIP, ZSTD, LZ4 is not implemented (placeholders copy data as-is)
- HuggingFace download logic is not implemented (returns error)
- Ollama proxy logic is present but depends on local service
- Some error handling and logging is minimal or stubbed

### MASM Migration Targets
- All decompression routines to be ported to MASM (`masm_decompressor.cpp`, `ggml_masm/compression.asm`)
- Model format routing and loading logic to be refactored for MASM64 compatibility
- Remove all external compression library dependencies in favor of MASM64 implementations
- Add MASM64 streaming and zone preloading for GGUF and other formats

### Next Steps
- [ ] Implement MASM decompression routines and wire into loader
- [ ] Refactor loader logic to call MASM routines for GGUF, HF, Ollama, and compressed formats
- [ ] Remove all external compression library dependencies
- [ ] Add regression and fuzz tests for MASM decompression and model loading
- [ ] Document all MASM entry points and test coverage
