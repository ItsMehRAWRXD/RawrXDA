# RawrXD IDE — strategic pillars

Canonical product themes. Use this for roadmap alignment, investor/partner briefs, and backlog triage. Pair with **`docs/IDE_MASTER_PROGRESS.md`** for measured progress and **`docs/BRIDGE_GAP_AUDIT.md`** for wiring gaps.

Last updated: 2026-03-20.

---

## 1. Native inference engine embedded in IDE (not API-dependent)

**Intent:** Run models **inside** the IDE process or co-located native runtime—**GGUF** load, decode, and diagnostics—without requiring a cloud API for core edit/assist flows.

**Repo signal (partial):** `GGUFRunner`, `RawrXD-TpsSmoke`, unified-memory path, model runtime gate; Ollama/backend URLs remain optional integration lanes. See **`docs/INFERENCE_PATH_MATRIX.md`**, **`docs/GGUF_UNIFIED_MEMORY.md`**, **`docs/GGUF_TENSOR_OFFSETS.md`**.

---

## 2. Autonomous SDLC pipeline (“Omega Orchestrator”)

**Intent:** End-to-end automation across plan → approve → execute → verify → ship, with explicit phases and observability (not a single opaque chat turn).

**Repo signal (partial):** Agentic planning orchestration + plan executor on Win32; Electron `bigdaddyg-ide` autonomous agent task flows. Historical **OmegaOrchestrator** references in verification memos—treat as **integration target**, not a single shipped binary name. See **`docs/AGENTIC_PLANNING_ORCHESTRATOR.md`**, **`docs/AUTONOMOUS_AGENT_ELECTRON.md`**, **`bigdaddyg-ide/AGENTIC_AUDIT_AND_WIRING.md`**.

---

## 3. Approval gate governance for AI actions

**Intent:** Every mutating or high-risk action passes through **policy + human or policy-approved** gates (workspace, shell, file writes, plan steps).

**Repo signal (in progress):** `Agentic::AgenticPlanningOrchestrator` approval queue, `PlanMutationGate` / `PlanRiskTier`, Win32 plan dialog sync (`applyWin32MutationGateSnapshot`, `syncActiveAgenticPlanApprovalsFromUi`). See **`docs/AGENTIC_PLANNING_ORCHESTRATOR.md`**.

---

## 4. Cross-session knowledge archaeology with Bayesian learning

**Intent:** Persist and **reuse** project/session signal across restarts—uncertainty-aware ranking of hypotheses, docs, and fixes (not raw log dumps only).

**Repo signal (vision / early):** SQLite/registry patterns and docs inventory exist; **full Bayesian cross-session learner** is not a single named subsystem in-tree—track as R&D pillar and break into concrete stores + UI surfaces.

---

## 5. Integrated reverse engineering suite (IDA-class tooling in an IDE)

**Intent:** Disasm, symbols, xrefs, and RE workflows **first-class** in the IDE, not a bolt-on external tool chain only.

**Repo signal (partial):** Reverse-engineering scripts, Codex/asm artifacts, sovereign examples, bridge audits. **Parity with IDA-class** remains a long-horizon goal; use **`docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md`** and repo `reverse_engineering_reports/` / extraction trees for scope.

---

## 6. Multi-GPU acceleration stack (AMD / Intel / ARM / Cerebras)

**Intent:** Schedule and load models across **heterogeneous** accelerators with honest memory reporting and fallbacks.

**Repo signal (partial):** Vulkan budget reporting, AMD unified memory toggle, GGUF + GPU paths in changelog and core. **Intel / ARM / Cerebras**—treat as expansion lanes; verify per-platform CMake targets and backends before claiming parity.

---

## 7. Enterprise license system with offline validation

**Intent:** Seat/tier/HWID-aware licensing with **air-gapped or degraded-network** operation via cache and grace rules.

**Repo signal (partial):** `EnterpriseLicenseV2`, `OfflineLicenseValidator` / cache paths, Win32 air-gapped enterprise UI flow. Online endpoints may be stubs—prefer **`validateWithFallback`** semantics until production PKI/server contract is fixed. See **`src/core/license_offline_validator.cpp`**, **`src/core/enterprise_licensev2_impl.cpp`**, **`src/win32app/Win32IDE_AirgappedEnterprise.cpp`**.

---

## How to use this doc

- **Backlog:** When opening an issue or batch doc, tag it with **Pillar N** from this list.
- **Partial → shippable:** **`docs/ENHANCEMENT_15_PRODUCT_READY_7.md`** — **15 enhancement tracks (E01–E15)** and **7 product-ready workstreams (PR01–PR07)** with acceptance-style “done when” gates.
- **Minimal surfaces:** **`docs/MINIMALISTIC_7_ENHANCEMENTS.md`** — **M01–M07** apply to every small feature as **additive** modifications (copy, persistence, empty/error, shortcuts, status, safe defaults, verify)—not full rewrites.
- **Audits:** `IDE_MASTER_PROGRESS` counts **done** work; this file states **north-star** intent so audits don’t drift into-only tactical fixes.
- **External lists:** If your private “~200 tasks” spreadsheet uses different wording, add a column **Pillar (1–7)** mapped to the rows here.
