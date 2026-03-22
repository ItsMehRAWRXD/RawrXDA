# Analysis & audit quick start (RawrXD)

**~5 minutes** — find the right audit or analysis doc without chasing stale Qt/agentic paths.

> **Note:** This repo’s **shipping IDE** is **Win32** (`RawrXD-Win32IDE`), not Qt. Older docs that mention `RawrXD-AgenticIDE`, `chat_interface.cpp`, or `RawrXD-QtShell` are **legacy / archive** unless explicitly refreshed.

---

## Two different “analysis” entry points

| You want | Open |
|----------|------|
| **GGUF tensor autopsy / model diff** (CLI, Vulkan wiring) | [`docs/ANALYSIS_QUICKSTART.md`](ANALYSIS_QUICKSTART.md) |
| **IDE / agentic / production gap audits** (this page) | Tables below |

---

## By goal

| I want to… | Start here |
|------------|------------|
| **Win32 IDE** feature / parity picture | [`WIN32_IDE_FEATURES_AUDIT.md`](WIN32_IDE_FEATURES_AUDIT.md), [`BRIDGE_GAP_AUDIT.md`](BRIDGE_GAP_AUDIT.md) |
| **Launch / reverse-engineer IDE** workflow | [`PROMPT_REVERSE_ENGINEER_IDE_LAUNCH_AUDIT.md`](PROMPT_REVERSE_ENGINEER_IDE_LAUNCH_AUDIT.md), [`IDE_LAUNCH_AUDIT_FINDINGS.md`](IDE_LAUNCH_AUDIT_FINDINGS.md) |
| **Agentic pipeline / stubs** | [`AGENTIC_PIPELINE_AUDIT.md`](AGENTIC_PIPELINE_AUDIT.md), [`AGENTIC_AUDIT_STUBS_AND_WIRING.md`](AGENTIC_AUDIT_STUBS_AND_WIRING.md) |
| **Commands / registry / diagnostics** | [`COMMAND_LEGACY_ID_AUDIT.md`](COMMAND_LEGACY_ID_AUDIT.md), [`FULL_SOURCE_DIAGNOSTICS_AUDIT_2026-03-20.md`](FULL_SOURCE_DIAGNOSTICS_AUDIT_2026-03-20.md) |
| **Production readiness (high level)** | [`PRODUCTION_READINESS_AUDIT.md`](PRODUCTION_READINESS_AUDIT.md) |
| **Historical “missing features” / competitive writeups** | [`archive/MISSING_FEATURES_AUDIT.md`](archive/MISSING_FEATURES_AUDIT.md), [`archive/COMPETITIVE_ANALYSIS.md`](archive/COMPETITIVE_ANALYSIS.md), [`archive/CRITICAL_MISSING_FEATURES_FIX_GUIDE.md`](archive/CRITICAL_MISSING_FEATURES_FIX_GUIDE.md) |
| **GGUF runner / TPS / offsets** | [`GGUF_TENSOR_OFFSETS.md`](GGUF_TENSOR_OFFSETS.md), [`INTEGRATED_RUNTIME.md`](INTEGRATED_RUNTIME.md), [`CHANGELOG.md`](CHANGELOG.md) (recent runner fixes) |

Root-level pointers (not under `docs/`): [`../README.md`](../README.md), [`../QUICK_REFERENCE.md`](../QUICK_REFERENCE.md), [`../QUICK_REFERENCE_CARD.txt`](../QUICK_REFERENCE_CARD.txt).

---

## Status snapshot (do not treat as CI truth)

Counts like “88 functions / 73% implemented” appear in **older** audit bundles (e.g. under `runoff/` or `docs/archive/`). They are **not** continuously regenerated. For “what blocks build today,” use a **fresh** `cmake --build` and the Win32 audit docs above.

---

## Critical path (practical order for this tree)

1. **Configure + build Win32 IDE** — `cmake` preset or `BUILD_IDE_FAST.ps1` / `BUILD_ORCHESTRATOR.ps1` as documented in [`README.md`](../README.md).
2. **Read `BRIDGE_GAP_AUDIT.md` + `WIN32_IDE_FEATURES_AUDIT.md`** — aligns expectations with actual Win32 surfaces.
3. **Run targeted tools** — e.g. `RawrXD-TpsSmoke`, `scripts/Run-TpsSmoke.ps1`, `scripts/Benchmark-TpsSmoke-Models.ps1` (see [`INTEGRATED_RUNTIME.md`](INTEGRATED_RUNTIME.md)).

---

## Key locations

```text
docs/
├── ANALYSIS_QUICKSTART.md          ← GGUF / ModelAnalysis CLI
├── ANALYSIS_AND_AUDIT_QUICKSTART.md ← This file (audit wayfinding)
├── BRIDGE_GAP_AUDIT.md
├── WIN32_IDE_FEATURES_AUDIT.md
├── INTEGRATED_RUNTIME.md
├── GGUF_TENSOR_OFFSETS.md
├── GGUF_UNIFIED_MEMORY.md
├── archive/                        ← Older competitive / missing-features / audit bundles
└── … (many *AUDIT*.md — search docs/ for your topic)
```

---

## Quick commands (Windows / PowerShell)

**Configure** (example; use your real build dir):

```powershell
cmake -S . -B build -G Ninja
```

**Build Win32 IDE:**

```powershell
cmake --build build --target RawrXD-Win32IDE -j 8
```

**Fast script** (if present in your clone):

```powershell
.\BUILD_IDE_FAST.ps1
```

**GGUF TPS harness** (after configuring `build_smoke_auto` or your dir):

```powershell
cmake --build build_smoke_auto --target RawrXD-TpsSmoke -j 8
.\scripts\Run-TpsSmoke.ps1 -SkipBuild
```

---

## Related

- [`.cursorrules`](../.cursorrules) — Win32 + `std::expected` + no Qt (project constraints)
- [`docs/IDE_MASTER_PROGRESS.md`](IDE_MASTER_PROGRESS.md) — progress ledger (if maintained in your branch)
