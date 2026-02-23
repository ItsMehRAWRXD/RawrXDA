# Pure x64 MASM Conversion Mapping — 367 Scaffold Markers

**Status:** In-progress  
**Last Updated:** 2026-02-16  
**Scope:** Five target modules → pure x64 MASM with no scaffold/partial/pre-production wording

---

## Five Target Modules

This document tracks the conversion of five C++ files to pure x64 MASM using the 367-marker system. Each module maps to existing or pending MASM implementations with no intermediate scaffolding.

---

## Module 1: Unified Overclock Governor

| Aspect | Details |
| ---- | ---- |
| **C++ Header** | `src/core/unified_overclock_governor.h` |
| **C++ Implementation** | None (header-only, will be replaced by MASM) |
| **MASM Implementation** | `src/asm/RawrXD_UnifiedOverclockGovernor.asm` (214 lines, pure x64) |
| **Scaffold Markers** | SCAFFOLD_226 (ASM build and ml64/nasm) · SCAFFOLD_196 (Toolchain) |
| **Status** | in_progress |
| **Purpose** | Hardware frequency control (CPU, GPU, RAM, NVMe/HDD) with auto-tune |
| **Extern C Interface** | OverclockGov_Initialize, OverclockGov_ApplyOffset, etc. |
| **Dependencies** | Zero external; pure Win32 + x64 ISA |

**Marker References Added:**
```cpp
// SCAFFOLD_226: ASM build and ml64/nasm
// SCAFFOLD_196: Toolchain (nasm, masm) and ASM run
// Pure x64 MASM implementation: src/asm/RawrXD_UnifiedOverclockGovernor.asm
```

---

## Module 2: Quantum Beaconism Backend

| Aspect | Details |
| ---- | ---- |
| **C++ Header** | `src/core/quantum_beaconism_backend.h` |
| **C++ Implementation** | None (header-only, will be replaced by MASM) |
| **MASM Implementations** | `src/asm/RawrXD_DualEngine_QuantumBeacon.asm` · `src/asm/quantum_beaconism_backend.asm` |
| **Scaffold Markers** | SCAFFOLD_134 (NEON/Vulkan fabric ASM) · SCAFFOLD_135 (MASM custom zlib) |
| **Status** | in_progress |
| **Purpose** | Fuse 10 Dual Engines into coherent backend with deterministic state-vector optimization |
| **Extern C Interface** | QuantumBeaconism_Fuse, QuantumBeaconism_Optimize, etc. |
| **Dependencies** | Zero external; deterministic RNG only |

**Marker References Added:**
```cpp
// SCAFFOLD_134: NEON/Vulkan fabric ASM
// SCAFFOLD_135: MASM custom zlib
// Pure x64 MASM implementations:
//   - src/asm/RawrXD_DualEngine_QuantumBeacon.asm
//   - src/asm/quantum_beaconism_backend.asm
```

---

## Module 3: Dual Engine System

| Aspect | Details |
| ---- | ---- |
| **C++ Header** | `src/core/dual_engine_system.h` |
| **C++ Implementation** | None (header-only, will be replaced by MASM) |
| **MASM Implementation** | `src/asm/RawrXD_DualAgent_Orchestrator.asm` (and supporting modules) |
| **Scaffold Markers** | SCAFFOLD_136 (inference_core.asm) · SCAFFOLD_137 (feature_dispatch_bridge.asm) |
| **Status** | in_progress |
| **Purpose** | 10x Dual Engine System (each = 2 CLI features, 20 features total) fused via Beaconism |
| **Extern C Interface** | DualEngine_InitAll, DualEngine_ExecuteOnAll, DualEngine_DispatchCLI, etc. |
| **Dependencies** | Zero external; pure Win32 + x64 ISA |

**Marker References Added:**
```cpp
// SCAFFOLD_136: inference_core.asm
// SCAFFOLD_137: feature_dispatch_bridge.asm
// Pure x64 MASM implementation: src/asm/RawrXD_DualAgent_Orchestrator.asm
```

---

## Module 4: Codebase Audit System

| Aspect | Details |
| ---- | ---- |
| **C++ Header** | `src/audit/codebase_audit_system.hpp` (660 lines) |
| **C++ Implementation** | `src/audit/codebase_audit_system.cpp` (1091 lines) |
| **MASM Implementation** | `src/asm/RawrXD_CodebaseAuditSystem.asm` |
| **Scaffold Markers** | SCAFFOLD_186 (Audit detect stubs) · SCAFFOLD_206 (Audit log checksum) · SCAFFOLD_325 (Stub vs production sweep) |
| **Status** | in_progress (C++ → MASM bridge) |
| **Purpose** | Code quality metrics, security pattern detection, performance analysis |
| **Current Role** | Interface layer; MASM accelerates analysis patterns and checksum operations |

**Marker References Added:**
```cpp
// SCAFFOLD_186: Audit detect stubs and report
// SCAFFOLD_206: Audit log immutable checksum
// SCAFFOLD_325: Stub vs production wording sweep
// SCAFFOLD_352: SOURCE_CODE_AUDIT Phase 2
// Bridges C++ interface to pure x64 MASM: src/asm/RawrXD_CodebaseAuditSystem.asm
```

---

## Module 5: (Not Found — User Reference Error)

- **Original request:** `D:\rawrxd\src\audit\codebase_audit_system_impl.cpp`
- **Actual files:** `src/audit/codebase_audit_system.hpp` and `codebase_audit_system.cpp` (no separate `_impl.cpp`)
- **Status:** Mapped to Module 4 above

---

## Marker Status Summary

The following SCAFFOLD markers have been marked as `in_progress` for this conversion work:

| Marker | Category | Description | Status |
| ---- | -------- | ----------- | ------ |
| SCAFFOLD_134 | Inference | NEON/Vulkan fabric ASM | in_progress |
| SCAFFOLD_135 | Inference | MASM custom zlib | in_progress |
| SCAFFOLD_136 | Inference | inference_core.asm | in_progress |
| SCAFFOLD_137 | Inference | feature_dispatch_bridge.asm | in_progress |
| SCAFFOLD_186 | CLI | Audit detect stubs and report | in_progress |
| SCAFFOLD_196 | CLI | Toolchain (nasm, masm) and ASM run | in_progress |
| SCAFFOLD_206 | Security | Audit log immutable checksum | in_progress |
| SCAFFOLD_226 | Build | ASM build and ml64/nasm | in_progress |
| SCAFFOLD_325 | Audit | Stub vs production wording sweep | in_progress |
| SCAFFOLD_352 | Audit | SOURCE_CODE_AUDIT Phase 2 | in_progress |

---

## File Changes Summary

### Headers Modified

1. **`src/core/unified_overclock_governor.h`**
   - Added SCAFFOLD_226, SCAFFOLD_196 references
   - Points to `src/asm/RawrXD_UnifiedOverclockGovernor.asm`

2. **`src/core/quantum_beaconism_backend.h`**
   - Added SCAFFOLD_134, SCAFFOLD_135 references
   - Points to `src/asm/RawrXD_DualEngine_QuantumBeacon.asm` and `src/asm/quantum_beaconism_backend.asm`

3. **`src/core/dual_engine_system.h`**
   - Added SCAFFOLD_136, SCAFFOLD_137 references
   - Points to `src/asm/RawrXD_DualAgent_Orchestrator.asm`

### Implementation Files Modified

4. **`src/audit/codebase_audit_system.hpp`**
   - Added SCAFFOLD_186, SCAFFOLD_206, SCAFFOLD_325, SCAFFOLD_352 references
   - Links to `src/asm/RawrXD_CodebaseAuditSystem.asm`

5. **`src/audit/codebase_audit_system.cpp`**
   - Added SCAFFOLD_186, SCAFFOLD_325 references
   - Notes MASM bridge pattern

### Registry Updated

6. **`SCAFFOLD_MARKERS.md`**
   - Marked 10 markers as `in_progress` (from `open`)
   - Updates reflect active pure MASM conversion work

---

## CMake Integration Points

These markers connect to the build system:

- **SCAFFOLD_226:** ASM build target (`ml64.exe` / `nasm`)
- **SCAFFOLD_196:** Toolchain registration and ASM run
- **SCAFFOLD_325:** `enforce_no_scaffold` target (verifies no partial scaffolds)

**Build command:**
```bash
cmake --build . --config Release --target RawrXD-Shell
```

---

## How to Verify Pure MASM Status

1. **Check marker comments** in each header file
2. **List MASM files:**
   ```powershell
   Get-ChildItem -Path d:\rawrxd\src\asm\RawrXD_*.asm | Sort-Object Name
   ```
3. **Verify CMake wiring:**
   ```bash
   cmake --build . --config Debug --target=list-asm-targets 2>&1 | grep -i "overclock\|quantum\|dual\|audit"
   ```
4. **Run audit:**
   ```bash
   d:\rawrxd\tools\enforce_no_scaffold.ps1 -ReportPath reports/ -Scope strict
   ```

---

## Next Steps (Not Yet Complete)

- [ ] Complete C++ stub removal for Modules 1–3 when MASM exports are ready
- [ ] Wire MASM exports via CMakeLists.txt for each module
- [ ] Validate ABI compatibility (calling convention, return types)
- [ ] Run `enforce_no_scaffold` to confirm zero scaffolding
- [ ] Update SCAFFOLD_MARKERS.md status to `done` when fully integrated

---

## References

- **SCAFFOLD_MARKERS.md** — Master registry (367 markers)
- **SCAFFOLD_MARKERS_CLARIFICATION.md** — How to use the marker system
- **include/scaffold_markers_367.h** — C++ header for marker IDs
- **tools/enforce_no_scaffold.ps1** — Verifies no partial scaffold wording

