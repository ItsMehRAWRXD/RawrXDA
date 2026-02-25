# Cursor & GitHub Parity — Production Spec (C++20 / x64 MASM)

**Goal:** All Cursor and GitHub parities are **included**, **fully useable**, and implemented in **pure C++20 or x64 MASM** with **maximum dependency removal** (DSA: data structures & algorithms in-house where reverse engineering allows).

---

## 1. Cursor parity (IDE feature set)

All of the following are exposed via **Cursor/JB Parity** menu and must be **production-ready**, **elegant**, and **C++20 or MASM-only** in the core path.

### 1.1 Telemetry export (8 commands)

| Command ID | Label | Implementation |
|------------|--------|----------------|
| IDM_TELEXPORT_JSON | Export JSON | `TelemetryExporter::ExportToFile(..., JSON)` — C++20 |
| IDM_TELEXPORT_CSV | Export CSV | Same, CSV format — C++20 |
| IDM_TELEXPORT_PROMETHEUS | Export Prometheus | Same, Prometheus text — C++20 |
| IDM_TELEXPORT_OTLP | Export OTLP | Same, OTLP — C++20 (no gRPC dep; HTTP/JSON or file) |
| IDM_TELEXPORT_AUDIT_LOG | Audit Log | `ExportAuditLog` — C++20 |
| IDM_TELEXPORT_VERIFY_CHAIN | Verify Chain | `VerifyAuditChain` — C++20 |
| IDM_TELEXPORT_AUTO_START/STOP | Auto-export | Start/stop timer — C++20 |

**Dep removal:** No external telemetry SDK; use Win32 timers and file I/O only.

### 1.2 Agentic composer UX (6 commands)

| Command ID | Label | Implementation |
|------------|--------|----------------|
| IDM_COMPOSER_NEW_SESSION | New Session | `AgenticComposerUX::StartSession` — C++20 |
| IDM_COMPOSER_END_SESSION | End Session | `EndSession` — C++20 |
| IDM_COMPOSER_APPROVE_ALL | Approve All | Batch approve file changes — C++20 |
| IDM_COMPOSER_REJECT_ALL | Reject All | Batch reject — C++20 |
| IDM_COMPOSER_SHOW_TRANSCRIPT | Show Transcript | Transcript UI — C++20 |
| IDM_COMPOSER_SHOW_METRICS | Show Metrics | Metrics panel — C++20 |

**Dep removal:** No Electron/Node in this path; all state in C++ (e.g. `AgenticComposerUX`), UI via Win32/Direct2D.

### 1.3 @-Mention context (4 commands)

| Command ID | Label | Implementation |
|------------|--------|----------------|
| IDM_MENTION_PARSE | Parse @-Mentions | `ContextMentionParser::Parse` — C++20 |
| IDM_MENTION_SUGGEST | Available Mentions | Suggest list — C++20 |
| IDM_MENTION_ASSEMBLE_CTX | Assemble Context | Build context blob for LLM — C++20 |
| IDM_MENTION_REGISTER_CUSTOM | Register Custom | Register custom mention handler — C++20 |

**Dep removal:** Pure string/AST parsing; no JS engine in hot path.

### 1.4 Vision encoder (4 commands)

| Command ID | Label | Implementation |
|------------|--------|----------------|
| IDM_VISION_LOAD_FILE | Load Image | Load image file → internal buffer — C++20 |
| IDM_VISION_PASTE_CLIPBOARD | Paste from Clipboard | Get DIB/PNG from clipboard — Win32 + C++20 |
| IDM_VISION_SCREENSHOT | Screenshot | GDI/Desktop Duplication — Win32 + C++20 |
| IDM_VISION_BUILD_PAYLOAD | Build Multimodal Payload | Encode for model API (base64 or raw) — C++20 |

**Dep removal:** No ImageMagick/Skia in required path; use Win32 GDI/DIB + minimal pixel handling; optional MASM for bulk base64 or pack.

### 1.5 Refactoring engine (9 commands)

| Command ID | Label | Implementation |
|------------|--------|----------------|
| IDM_REFACTOR_EXTRACT_METHOD | Extract Method | RefactoringPlugin — C++20 |
| IDM_REFACTOR_EXTRACT_VARIABLE | Extract Variable | Same |
| IDM_REFACTOR_RENAME_SYMBOL | Rename Symbol | Same |
| IDM_REFACTOR_ORGANIZE_INCLUDES | Organize Includes | Same |
| IDM_REFACTOR_CONVERT_AUTO | Convert to auto | Same |
| IDM_REFACTOR_REMOVE_DEAD_CODE | Remove Dead Code | Same |
| IDM_REFACTOR_SHOW_ALL | Show All Refactorings | List from registry — C++20 |
| IDM_REFACTOR_LOAD_PLUGIN | Load Plugin | Load DLL plugin — Win32 + C++20 |

**Dep removal:** Plugin ABI is C-only or C++20; no Node/npm; optional MASM for hot text-scanner paths.

### 1.6 Language registry (4 commands)

| Command ID | Label | Implementation |
|------------|--------|----------------|
| IDM_LANG_DETECT | Detect Language | LanguagePlugin registry — C++20 |
| IDM_LANG_LIST_ALL | List All Languages | Enumerate registry — C++20 |
| IDM_LANG_LOAD_PLUGIN | Load Plugin | DLL load — Win32 |
| IDM_LANG_SET_FOR_FILE | Set Language for File | Associate buffer to language — C++20 |

**Dep removal:** No external language detection service; in-process DLL or built-in list only.

### 1.7 Semantic index (9 commands)

| Command ID | Label | Implementation |
|------------|--------|----------------|
| IDM_SEMANTIC_BUILD_INDEX | Build Index | SemanticIndexEngine — C++20 (or MASM for hot scan) |
| IDM_SEMANTIC_FUZZY_SEARCH | Fuzzy Search | In-memory index — C++20 |
| IDM_SEMANTIC_FIND_REFS | Find References | Index lookup — C++20 |
| IDM_SEMANTIC_SHOW_DEPS | Show Dependencies | Graph from index — C++20 |
| IDM_SEMANTIC_TYPE_HIERARCHY | Type Hierarchy | From index — C++20 |
| IDM_SEMANTIC_CALL_GRAPH | Call Graph | From index — C++20 |
| IDM_SEMANTIC_FIND_CYCLES | Find Cycles | DFA on dependency graph — C++20 |
| IDM_SEMANTIC_LOAD_PLUGIN | Load Plugin | DLL — Win32 |

**Dep removal:** Index is in-process; no LSP server required for index build (optional LSP for live diagnostics only); hot loops (tokenize, hash) can use MASM.

### 1.8 Resource generator (5 commands)

| Command ID | Label | Implementation |
|------------|--------|----------------|
| IDM_RESOURCE_GENERATE | Generate Resource | ResourceGeneratorEngine — C++20 |
| IDM_RESOURCE_GEN_PROJECT | Generate Project Scaffold | Templates — C++20 |
| IDM_RESOURCE_LIST_TEMPLATES | List Templates | Enumerate — C++20 |
| IDM_RESOURCE_SEARCH | Search Templates | In-memory search — C++20 |
| IDM_RESOURCE_LOAD_PLUGIN | Load Plugin | DLL — Win32 |

**Dep removal:** No npm/yeoman; templates are files + C++20 string/token replacement.

---

## 2. GitHub parity

All GitHub-related features must be **useable in production** and implemented with **C++20 + WinHTTP** (no curl in critical path); JSON in update path is **manual parse** (already in `update_signature.cpp`).

### 2.1 Releases API (read)

| Feature | Implementation | Dep |
|--------|-----------------|-----|
| Fetch latest release | `UpdateSignatureVerifier::fetchManifest()` — WinHTTP GET `api.github.com/repos/.../releases/latest` | WinHTTP only; manual JSON parse |
| Parse version, tag, release notes, files[] | `parseManifest()` — manual `jsonExtractString` / `jsonExtractUint64` | No nlohmann in update path |

**Status:** Already in `src/core/update_signature.cpp` (WinHTTP + manual JSON). Keep it; no new deps.

### 2.2 Releases API (write)

| Feature | Implementation | Dep |
|--------|-----------------|-----|
| Create GitHub release | `ReleaseAgent::createGitHubRelease(tag, changelog)` — POST with `GITHUB_TOKEN` | WinHTTP + manual JSON build |
| Upload asset (optional) | Same agent; WinHTTP PUT to release upload URL | WinHTTP only |

**Status:** Stub in `src/agent/release_agent.cpp`; production path: implement POST with WinHTTP, build JSON with `JsonDoc::toJson` or manual string concat; no curl.

### 2.3 Authentication

| Feature | Implementation |
|--------|-----------------|
| Token from env | `getEnv("GITHUB_TOKEN")` — C++20 |
| Optional: credential store | Win32 Credential Manager API — no third-party lib |

**Dep removal:** No libcurl, no OpenSSL in minimal path (use WinHTTP + BCrypt for TLS/crypto where needed).

---

## 3. Implementation rules (elegant, production, max dep removal)

### 3.1 Language split

- **C++20:** All control flow, data structures, UI orchestration, plugin loading, Win32 API calls, WinHTTP, file I/O, parsing (manual JSON where required in update/release path).
- **x64 MASM:** Where reverse engineering and performance justify it: hot loops (e.g. inference kernels, compression, byte search, tokenization), memory patching, custom codec (e.g. brutal_gzip), disassembler (RawrCodex). Call from C++ via `extern "C"` or explicit ASM object linkage.

### 3.2 Dependency removal (DSA)

- **HTTP:** WinHTTP only in update/release and GitHub API paths; no curl. Optional: keep curl only for non-critical or legacy paths if documented.
- **JSON (critical path):** Update manifest and GitHub release payload: manual parsing/serialization only (as in `update_signature.cpp`). nlohmann allowed elsewhere for non-update features.
- **Telemetry:** No external SDK; use `UnifiedTelemetryCore` + file export (C++20).
- **Compression:** Prefer `brutal_gzip` (MASM/C++) over zlib where integrated; max dep removal for update/installer payloads.
- **Crypto:** BCrypt/CNG (Win32) for signature verification; no OpenSSL in minimal update path.
- **Plugins:** Win32 LoadLibrary/GetProcAddress; ABI C or C++20; no Node/QuickJS in parity command path (QuickJS only for optional extension script layer, not for Cursor parity menu).

### 3.3 Production “fully useable”

- Every menu item in **Cursor/JB Parity** and every GitHub feature above must:
  - Be **wired** (command ID → handler → module).
  - **Not** be a no-op unless explicitly “stub for future.”
  - Prefer **elegant** APIs: `std::expected` or `PatchResult`-style results; no exceptions in critical paths; clear error messages.

---

## 4. Checklist

- [ ] All 8 Cursor parity modules initialized in `initAllCursorParityModules()`.
- [ ] All Cursor parity command IDs (11500–11574) handled in `handleCursorParityCommand()`.
- [ ] GitHub: fetch latest release (WinHTTP + manual JSON) — done in `update_signature.cpp`.
- [ ] GitHub: create release (WinHTTP POST + token) — implement in `release_agent.cpp` when needed.
- [ ] No curl in update/release/GitHub critical path.
- [ ] No nlohmann in update manifest parse path.
- [ ] Hot paths (inference, compression, search) use C++20 or MASM; MASM where reverse engineering allows and perf matters.

---

## 5. File reference

| Area | Files |
|------|--------|
| Cursor parity menu & init | `src/win32app/Win32IDE_CursorParity.cpp`, `Win32IDE.h` (createCursorParityMenu, handleCursorParityCommand, initAllCursorParityModules) |
| Telemetry export | `include/telemetry/telemetry_export.h` |
| Agentic composer | `include/agentic/agentic_composer_ux.h`, `src/agentic/agentic_composer_ux.cpp` |
| Context @-mentions | `include/context/context_mention_parser.h` |
| Vision | `include/multimodal/vision_encoder.h` |
| Refactoring | `include/ide/refactoring_plugin.h` |
| Language | `include/ide/language_plugin.h` |
| Semantic index | `include/context/semantic_index.h` |
| Resource generator | `include/ide/resource_generator.h` |
| Update / GitHub (read) | `src/core/update_signature.cpp`, `include/update_signature.h` |
| GitHub (write) | `src/agent/release_agent.cpp`, `include/agent/release_agent.hpp` |

This spec ensures **all Cursor and GitHub parities are included and fully useable** in a **production**, **elegant** way via **pure C++20 or x64 MASM**, with **maximum dependency removal** and DSA in-house where reverse engineering allows.
