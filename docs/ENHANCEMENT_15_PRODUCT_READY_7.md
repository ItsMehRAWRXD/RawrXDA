# 15× enhancement backlog + 7× product-ready code

Turns **partial** areas from **`docs/IDE_STRATEGIC_PILLARS.md`** and **`docs/BRIDGE_GAP_AUDIT.md`** into **actionable** work: **15 enhancement tracks (E01–E15)** and **7 product-readiness workstreams (PR01–PR07)**.

Use **E** items for capability multiplier / user-visible depth; use **PR** items for shippable quality (build, safety, supportability).

Last updated: 2026-03-20.

---

## Snapshot: partial → target

| Pillar | Today | After E* + PR* |
|--------|--------|----------------|
| 1 Native inference | GGUF + gates; parallel paths | One **facade** + native-default UX; matrix-tested |
| 2 Omega / SDLC | Plans + Electron agent; fragmented names | **Named phase machine** + shared observability contract |
| 3 Approval gates | Win32 + orchestrator sync | **Policy file + audit trail**; tools wired, not simulated |
| 4 Knowledge / Bayesian | Inventory / SQLite elsewhere | **Project knowledge DB** + **Bayesian-lite** ranking |
| 5 RE suite | Scripts, asm, sovereign bits | **MVP panels** (symbols/xrefs/disasm provider) |
| 6 Multi-GPU | AMD + Vulkan budget | **Discovery + policy table** + documented Intel/ARM lanes |
| 7 Enterprise license | V2 + offline cache; stub online | **Production crypto contract** + admin/support bundle |

---

## Part A — 15 enhancement tracks (E01–E15)

Each row: **ID**, **pillar**, **outcome**, **primary leverage**, **done when** (acceptance).

| ID | Pillar | Outcome | Primary leverage | Done when |
|----|--------|---------|------------------|-----------|
| **E01** | 1 | **Single inference façade** for “user action → engine” | `INFERENCE_PATH_MATRIX.md` + backend switcher + `GGUFRunner` | Matrix doc matches code; no silent path bypass; failures surface one error channel. |
| **E02** | 1 | **GGUF production depth** | `GGUFRunner`, `gguf_k_quants`, TpsSmoke | IQ-family or documented exclusion; batch script green on N reference models; RAM caps honored. |
| **E03** | 1 | **Native-default product posture** | Settings, chat panel, agentic bridge | When local GGUF loaded, UI default route is native unless user overrides; cloud is opt-in per workspace. |
| **E04** | 2 | **SDLC phase machine (“Omega”)** | Plan executor, orchestrator, logs | Explicit phases (e.g. plan → mutate → verify → ship) with phase enum + structured log field; no anonymous long run. |
| **E05** | 2,3 | **Real tool execution on approved steps** | `AgenticPlanningOrchestrator`, `AgenticBridge`, plan worker | `m_toolExecFn` drives real mutations; failure stops plan; rollback aligned with file backups. |
| **E06** | 2 | **Cross-stack autonomous contract** | `bigdaddyg-ide`, `AUTONOMOUS_AGENT_ELECTRON.md`, Win32 parity doc | One table: IPC events, gates, and idempotency rules shared by Electron + native doc. |
| **E07** | 3 | **Machine-readable policy + audit** | SQLite/registry, plan dialog | `approval_policy.json` (or equiv) + append-only **audit** table: who/when/step/workspace hash. |
| **E08** | 4 | **Cross-session knowledge store** | Existing SQLite patterns, project root | Per-project DB: sessions, artifacts, hypotheses, doc refs; survives IDE restart. |
| **E09** | 4 | **Bayesian-lite layer** | Knowledge store + failure hooks | Per **error signature**: Beta prior on “fix worked”; UI ranks top-k hypotheses; no black-box-only chat. |
| **E10** | 5 | **RE MVP: symbols + xrefs shell** | Codex/asm pipeline, sovereign examples | Dockable panel: load artifact → symbol list → xref list (even if read-only v1). |
| **E11** | 5 | **Disasm provider abstraction** | `IEditorEngine` / Monaco lane | Interface + one backend (existing or Capstone-shaped); swap without rewriting UI. |
| **E12** | 6 | **Device discovery + VRAM truth table** | `GPUBackend`, AMDGPU, Vulkan | At startup: enumerated devices, budget, chosen lane; logged + shown in AI/Model diagnostics. |
| **E13** | 6 | **Heterogeneous policy table** | CMake + backend registry | Documented matrix: AMD / Intel / ARM / “external” with feature flags; no fake “supported” in UI. |
| **E14** | 7 | **Production license cryptography** | `enterprise_licensev2_impl`, offline validator | Signing keys not dev placeholders; online endpoint contract documented; grace/blacklist tested. |
| **E15** | 7 | **Enterprise operator UX** | `Win32IDE_AirgappedEnterprise`, exports | Admin can export **compliance bundle** (HWID, tier, last validation, cache state) for support tickets. |

**Suggested sequencing (dependency-aware):** E01 → E03 → E02; E05 → E04 → E06; E07 parallel to E05; E08 → E09; E10 → E11; E12 → E13; E14 → E15.

---

## Part C — Delivery log (repo implementation batches)

| Batch | Tracks | Primary artifacts |
|-------|--------|-------------------|
| **1** | E01, E03, E02, E05, E07, E04, E06 | `docs/INFERENCE_FACADE_CONTRACT.md`, `docs/GGUF_PRODUCTION_DEPTH.md`, matrix link; Electron `preferLocalInferenceFirst` + settings; `approval_policy.json` + `approval_audit.js` + IPC audit; `orchestrator.js` `omegaPhase` + halt on mutate fail; `CROSS_STACK_AUTONOMOUS_CONTRACT.md` |
| **2** | E08–E14 | `knowledge_store.js` + IPC; docs: knowledge, Bayesian, RE MVP, disasm, GPU discovery, heterogeneous policy, license crypto |
| **3** | E15 | `enterprise:export-compliance-bundle` IPC, preload, Settings button; `ENTERPRISE_COMPLIANCE_BUNDLE.md` |

*PR01–PR07 remain separate quality gates; extend this table when each PR row is satisfied.*

---

## Part B — 7× product-ready code (PR01–PR07)

These are **quality multipliers** on top of E-items: what makes the binary **safe to ship** and **supportable**.

| ID | Theme | Deliverables | Ship gate |
|----|--------|--------------|-----------|
| **PR01** | **Build + CI truth** | Win32 IDE + smoke targets always green on `master`; optional `RawrXD-TpsSmoke` in CI | Broken main is unacceptable; artifact names/version in build log. |
| **PR02** | **Observability** | Correlation id (session/run) across OrchestratorBridge, plan executor, GGUF load | Support can grep one id across logs; doc field list in `IDE_MASTER_PROGRESS` or runbook. |
| **PR03** | **Unified error surface** | `std::expected` / structured errors from inference + agent loops to UI + HTTP | Same failure class → same user string + same log code (bridge gap **H** closed). |
| **PR04** | **Security pass** | Egress defaults, secret scan, license HWID handling, extension load path | Checklist signed off; no dev keys in release path (pairs with **E14**). |
| **PR05** | **Test floor** | Catch2 (or harness) for: license offline path, GGUF header parse, policy gate parser | Minimum tests run in CI; regressions caught before manual QA. |
| **PR06** | **Runbooks** | Install, “IDE won’t start”, model load failure, license offline — one page each | Linked from README or `docs/` index; stale docs flagged in audits. |
| **PR07** | **Release engineering** | Feature manifest matches CMake lanes; stubs explicit in `Win32IDE_FeatureManifest` | No silent stub in production without UI/industry disclosure (bridge **F** policy). |

---

## How to run this in the repo

1. **Tag work:** In commits/issues, use `E##` and/or `PR##` in the subject.
2. **Reconcile counts:** Bump **`docs/IDE_MASTER_PROGRESS.md`** when an E-row’s “done when” is satisfied.
3. **Pillars stay canonical:** **`docs/IDE_STRATEGIC_PILLARS.md`** = *why*; this file = *what to build next*.

---

## Related docs

- **`docs/IDE_STRATEGIC_PILLARS.md`** — seven north-star themes  
- **`docs/BRIDGE_GAP_AUDIT.md`** — wiring gaps (feeds E01, E05, PR03)  
- **`docs/INFERENCE_PATH_MATRIX.md`** — path unification (E01)  
- **`docs/AGENTIC_PLANNING_ORCHESTRATOR.md`** — gates + orchestrator (E05, E07)  
- **`docs/IRC_MIRC_IDE_BRIDGE.md`** + **`bigdaddyg-ide/electron/WIRING_IRC_BRIDGE.md`** — IRC remote control + preload/main/protocol wiring (E06 adjunct, PR06 runbook material)  
- **`docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md`** — RE scope (E10–E11)  
- **`docs/INFERENCE_FACADE_CONTRACT.md`** (E01) · **`docs/GGUF_PRODUCTION_DEPTH.md`** (E02) · **`docs/CROSS_STACK_AUTONOMOUS_CONTRACT.md`** (E06) · **`docs/PROJECT_KNOWLEDGE_STORE.md`** (E08) · **`docs/BAYESIAN_LITE_HYPOTHESES.md`** (E09) · **`docs/ENTERPRISE_COMPLIANCE_BUNDLE.md`** (E15)  
- **`docs/MINIMALISTIC_7_ENHANCEMENTS.md`** — **M01–M07** additive polish checklist for **every** small UI / IPC surface (tooltips, persistence, empty/error, keyboard, status, safe defaults, verify path); extends minimal features without replacing them.  
