# Master Audit Index - RawrXD IDE
**Location:** D:\lazy init ide\src\
**Goal:** Full reverse engineering and MASM64 migration

## Audit Status

### Completed Audits
✅ **agentic/** - Production-ready, ready for MASM64 migration
   - [Audit Report](agentic/AUDIT.md)
   - 3 files, 919 lines total
   - Status: Production-ready
   - Next: MASM64 migration

### Pending Audits (In Priority Order)

#### Phase 1: Core GPU/Compute Dependencies
🔄 **gpu_masm/** - MASM GPU backend (exists)
   - Status: Exists with real MASM backends (see `AUDIT.md`, `gpu_backend.asm`, `gpu_detection.asm`, `vulkan/`)
   - Gaps: Export C bindings; finish advanced property extraction in `gpu_detection.asm`
   - Priority: CRITICAL
   - Dependencies: None (pure MASM)

🔄 **gpu/** - GPU backend abstraction (C++ stubs)
   - Files: gpu_backend.cpp, kv_cache_optimizer.cpp, speculative_decoder.cpp
   - Priority: CRITICAL
   - Findings: Backend init is stubbed (Vulkan always succeeds, CUDA always fails); cache + speculative decoder are CPU-only placeholders
   - Action: Wire to `gpu_masm` entry points, add tests, remove placeholder logic

🔄 **ggml_masm/** - TO BE CREATED
   - Status: Does not exist, needs creation
   - Priority: CRITICAL
   - Dependencies: None (new implementation)

🔄 **ggml*/** - GGML library files
   - Files: ggml.c, ggml.cpp, ggml-quants.c, etc.
   - Priority: CRITICAL
   - Dependencies: BLAS, GPU libraries (to be replaced)

🔄 **vulkan_compute/** - Vulkan compute implementation
   - Files: vulkan_compute.cpp, vulkan_compute_stub.cpp
   - Priority: HIGH
   - Dependencies: Vulkan SDK (to be replaced)

🔄 **vulkan_stubs.cpp** - Vulkan stubs
   - Priority: HIGH
   - Dependencies: Vulkan SDK (to be replaced)

#### Phase 2: Model Loading & Inference
⏳ **model_loader/** - Model loading logic
   - Priority: HIGH
   - Dependencies: GGML, file formats

⏳ **inference_engine*/** - Inference engines
   - Priority: HIGH
   - Dependencies: GGML, GPU backends

⏳ **transformer*/** - Transformer implementations
   - Priority: MEDIUM
   - Dependencies: GGML, model loaders

#### Phase 3: Networking & Compression
⏳ **net/** - Networking code
   - Priority: MEDIUM
   - Dependencies: libcurl (to be replaced)

⏳ **codec/** - Compression codecs
   - Priority: MEDIUM
   - Dependencies: Zlib, Zstd (to be replaced)

⏳ **masm_decompressor.cpp** - MASM decompression
   - Priority: LOW (already MASM)
   - Dependencies: None

#### Phase 4: UI & Integration
⏳ **ui/** - User interface
   - Priority: LOW
   - Dependencies: Qt (to keep for GUI)

⏳ **gui/** - GUI components
   - Priority: LOW
   - Dependencies: Qt (to keep for GUI)

⏳ **cli/** - Command-line interface
   - Priority: MEDIUM
   - Dependencies: Standard library

#### Phase 5: Agentic Systems
⏳ **autonomous*/** - Autonomous systems
   - Priority: MEDIUM
   - Dependencies: Internal

## Implementation Strategy

### For Each Folder:
1. Create `FOLDER_AUDIT.md` documenting current state
2. Identify all external dependencies
3. Create MASM64 implementation plan
4. Implement MASM64 replacements
5. Update CMakeLists.txt to remove external dependencies
6. Test and validate

### MASM64 Implementation Priority
1. **Critical Path:** GPU backends, GGML core
2. **High Impact:** Model loading, inference
3. **Medium Impact:** Networking, compression
4. **Low Impact:** UI components (Qt to keep)

## Next Steps
1. ✅ Complete audit of `agentic/`
2. 🔄 Audit `gpu/` (stubs identified; see `gpu/GPU_FOLDER_AUDIT.md` and `gpu/GPU_BACKEND_AUDIT.md`)
3. ✅ `gpu_masm/` exists; next: expose C bindings and finish property extraction
4. 🔄 Start MASM64 integration (bridge `gpu/` C++ to `gpu_masm/`; add tests)

## Notes
- All Qt dependencies in UI folders will be kept (essential for GUI)
- Win32 API calls will be translated to MASM64
- External compute libraries (Vulkan, CUDA, ROCm) will be replaced
- Goal: Pure MASM64 implementation with GPU acceleration
