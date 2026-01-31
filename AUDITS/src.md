# Src Folder Audit (D lazy init)

Status overview
- Core planning path: PlanOrchestrator integrated with InferenceEngine; JSON parsing and validation present.
- IDE wiring: ProductionAgenticIDE handlers implemented; build shows header inclusions required for Qt widgets (e.g., QMenuBar/QStatusBar) and unmatched brace scope.
- Inference: Transformer + ggml with Vulkan backend enabled; CPU fallback intact.
- Tokenizers: BPE and SentencePiece present.
- GGUF loader: Compression path links to codec::deflate/inflate and deflate_brutal_masm; link errors indicate missing implementations or linkage.

Pending work
- Fix Qt includes and scope in `src/production_agentic_ide.cpp` (add `<QMenuBar>`, `<QStatusBar>`, verify class scope braces).
- Provide MASM implementations for `deflate_brutal_masm` and codec glue or disable those features via config.
- Resolve `centralized_exception_handler.h` include in `src/qtapp/main_qt.cpp`.
- Address `TaskOrchestrator` types in include/orchestration (ensure headers present and included).

GPU plan
- Vulkan compute reachable; CUDA/ROCm disabled. Proceed with MASM GPU scaffolding for Vulkan interop and eventual ROCm parity.

Path normalization
- Work from `D:\lazy init ide` with fresh build. Avoid absolute `E:\` references; prefer relative paths and environment variable `LAZY_INIT_IDE_ROOT`.
