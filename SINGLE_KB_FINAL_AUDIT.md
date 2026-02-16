# SINGLE_KB_FINAL_AUDIT (CODE ONLY)

Date: 2026-02-16
Branch: cursor/final-kb-audit-bd84

This pass intentionally ignores README/manual/instruction documents and uses only code and build files.

Scope:
1) exact single KiB: size == 1024 bytes
2) single-KiB band: 1024 <= size < 2048 bytes

Code-only metrics:
- files in 1-2 KiB band: 467
- files exactly 1024 bytes: 2
- suspicious code/build files in 1-2 KiB band: 44
- high-NUL files in 1-2 KiB band (>=80 percent NUL): 23

Exact 1024-byte code files:
1) src/ggml-vulkan/vulkan-shaders/add_id.comp
2) 3rdparty/ggml/src/ggml-vulkan/vulkan-shaders/add_id.comp

Finding severity (code-path evidence only):

High finding 1:
- file: src/masm/interconnect/RawrXD_Model_StateMachine.asm:25-28
- issue: stub returns lea rax, [rsp] as a model instance pointer
- impact: returned pointer points to stack frame and is invalid after return
- build wiring: build_masm_interconnect.bat:39 and build_interconnect_complete.bat:21 and src/build_rawrxd_asm.bat:10

High finding 2:
- file: src/digestion/RawrXD_DigestionEngine.asm:33-35
- issue: TODO core path but returns success for non-null input
- impact: false success in digestion flow, work can be reported complete when logic is not implemented
- build wiring: src/digestion/CMakeLists.txt:28-31

Medium finding 1:
- file: src/agentic/vulkan/NEON_VULKAN_FABRIC_STUB.asm:16-43
- issue: exported API returns success/no-op values
- impact: behavior can look healthy while real Neon/Vulkan logic is absent
- build wiring: src/agentic/CMakeLists.txt:57-61

Medium finding 2:
- file: src/win32app/reverse_engineered_stubs.cpp:20-45
- issue: reverse-engineered bridge symbols implemented as no-op/minimal fallbacks
- impact: real kernel path can be masked by fallback behavior
- build wiring: CMakeLists.txt:1610

Medium finding 3:
- file: src/stubs.cpp:13-17
- issue: fallback engine registration path
- impact: subsystem can run in fallback mode without hard failure signal
- build wiring: CMakeLists.txt:204 and CMakeLists.txt:1405 and CMakeLists.txt:1577 and src/inference/CMakeLists.txt:69

Medium finding 4:
- files: compilers/_patched/*.asm and src/reverse_engineering/reverser_compiler/reverser_*.asm
- issue: 23 files in the 1-2 KiB band are mostly NUL bytes (many about 96 percent, some 100 percent)
- impact: accidental inclusion can break assembly pipeline or produce non-deterministic failures
- note: current active CMake wiring was not found for these specific patched files, but they are dangerous artifacts in-tree

Low finding:
- exact-1-KiB shader duplication exists in src and vendored 3rdparty copy
- both files are byte-identical in this audit pass
- risk is drift risk only if one copy changes without sync

Direct code-only verdict:
- exact 1 KiB code files show no direct defect
- the real risk is in 1-2 KiB stub files that are build-wired in active targets
- immediate fixes should start with RawrXD_Model_StateMachine.asm and RawrXD_DigestionEngine.asm
