# From-Scratch Policy — In-House Solutions Only

**Goal:** Prefer our own implementations over third-party libraries. No new external deps for core, security, IDE, or toolchain features.

---

## 1. What Counts as "In-House"

| Category | In-house | Notes |
|----------|----------|--------|
| **Runtime / CRT** | MSVC static CRT, Windows SDK | Platform; we do not ship a custom CRT. |
| **Build** | CMake, Ninja, MSVC/cl.exe, MASM64 | Compiler/toolchain is platform or our Phase 1/2. |
| **JSON** | `src/core/rawrxd_json.hpp` | Minimal parse/dump for new code; eventual replacement for nlohmann in new modules. |
| **Security** | `secrets_scanner`, `sast_rule_engine`, `dependency_audit`, `tamper_detection` | No SonarQube, no OpenSSL for scanning (hashing in tamper is in-house or SDK). |
| **RE / Binary** | RawrCodex, RawrDumpBin, Phase 1/2, static_analysis_engine | Our own disasm, CFG, SSA, PE writer. |
| **Inference** | ggml (in-tree fork), AVX512 kernels, RawrBlob | Model loader and CPU inference in-house. |
| **IDE** | Win32 API only; no Qt, no Electron | Native UI and LSP client in-house. |
| **Debugger** | DbgEng (Windows SDK) | We use the OS debug API; no custom debug engine. |

---

## 2. Allowed External / Platform

- **Windows SDK** (Win32, DbgEng, Winsock, WinHTTP)
- **MSVC** (compiler, linker, CRT)
- **CMake + Ninja** (build)
- **Threads** (C++ std::thread / system)
- **Vulkan** (optional, for GPU inference; can be OFF)
- **ggml** (bundled in-tree; we maintain it)

---

## 3. No New External Deps For

- **Security:** SAST rules, secrets scan, dependency/SCA, SBOM — all implemented in `src/security/` with STL + our code.
- **Config / API:** New code should use `rawrxd_json` (or string parsing) instead of adding nlohmann for new files.
- **Parsing:** Manifest parsing (package.json, requirements.txt, etc.) is string/line-based or uses `rawrxd_json`; no new JSON libs.
- **RE / Compiler:** No LLVM/clang dependency for our toolchain; Phase 1 assembler + Phase 2 linker are from scratch.

---

## 4. Existing Third-Party and Migration

| Current | Replacement / Plan |
|---------|--------------------|
| **nlohmann/json** (bundled) | New code: `rawrxd_json.hpp`. Existing code: migrate when touching; no big-bang replace. |
| **OpenSSL** (license_creator only) | Keep for signing only; no use in core/security scanning. |
| **Crow / Boost / SQLite** (extension_registry) | Isolated; core IDE and CLI do not depend on them. |
| **Vulkan/CUDA/HIP/etc.** | Optional backends; CPU path is zero external. |

---

## 5. In-House Modules (No Other Deps)

- `src/core/rawrxd_json.hpp` — minimal JSON parse/dump (object, array, string, number, bool, null).
- `src/core/problems_aggregator.cpp` — unified problems; STL only.
- `src/security/secrets_scanner.cpp` — regex + entropy; STL only.
- `src/security/dependency_audit.cpp` — manifest parsing (string/line or rawrxd_json); reports to ProblemsAggregator.
- `src/security/sast_rule_engine.cpp` — regex + line/source rules; reports to ProblemsAggregator.
- `toolchain/from_scratch/` — Phase 1 assembler, Phase 2 linker; no LLVM.

All of the above use only C++ STL, Windows SDK where needed, and our own headers — no other deps.
