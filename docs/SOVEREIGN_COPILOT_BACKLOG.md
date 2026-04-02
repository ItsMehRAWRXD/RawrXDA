# Sovereign Copilot backlog — milestones and BOM

Operational envelope for a **solo** developer, **Win32-first** IDE, **headless inference** decoupled from UI, and **medium MoE GGUF** as the first “real” model class. This file is the canonical milestone list; CI job names and commands should stay in sync with it.

---

## Locked configuration

| Dimension | Choice | Notes |
|-----------|--------|--------|
| Team | Solo (+ AI / automation) | Breadth limited; CI sweeps (`PostProcess-MoeGroupedGemmSweep`, numerical gate) replace extra headcount. |
| Deployment | Win32 primary; headless engine sidecar | UI must stay responsive (60 FPS target); inference runs out-of-process at raised priority. |
| IPC (v1) | Shared memory (file mapping) **or** named pipe | Prefer **shared memory + sequence numbers** for bulk tensors/logits; **named pipe or secondary control pipe** for cancel/session if needed. |
| First production model class | ~8×7B MoE, **Q4_K_M GGUF** | Swarm keeps a **subset** of experts VRAM-resident; backbone and cold experts from RAM/disk. Exact on-disk size varies by build—treat as a **policy** target, not a hard byte cap. |
| First CI model class | Tiny MoE | Fast parity and regression; exact filename below. |

---

## Bill of materials (BOM)

### Build targets (CMake names today)

| Role | CMake target | Purpose |
|------|----------------|---------|
| Headless inference / swarm / fused math | `RawrXD-InferenceEngine` | Sidecar process; load GGUF, run forward, stream tokens over IPC. |
| Win32 IDE | `RawrXD-Win32IDE` | Editor, heatmap, ghost text, session UI. |

**Shipping alias (optional):** Rename or copy at install time to `RawrXD_Inference_Service.exe` and `RawrXD_IDE_Frontend.exe` if you want stable external names; keep CMake targets as-is to avoid churn.

### Test models (local / CI)

| Model | Example filename | Use |
|-------|------------------|-----|
| Primary dogfood MoE | `mixtral-8x7b-v0.1.Q4_K_M.gguf` (or equivalent 8×7B-class) | Latency, TTFT, swarm residency tuning. **Not committed** (`.gitignore` covers `*.gguf`). |
| Numerical / parity gate | `tiny-moe-2x128.gguf` (or repo-documented substitute) | CI mixture parity, small K/hidden for fast runs. Store path in env or CI secret/cache. |

### CI gates

| Workflow file | Name (GitHub UI) | Purpose |
|---------------|------------------|---------|
| `.github/workflows/moe-numerical-gate.yml` | MoE numerical gate | Fused / grouped path vs reference; policy + pack tests as wired in workflow. |
| `.github/workflows/latency-regression-gate.yml` | *(to add)* Latency regression gate | Fail if microbench metrics (e.g. `avg_us_gemm`) regress > agreed threshold vs baseline artifact. |

---

## Milestones (ordered)

### M0 — IPC contract: Shared Memory Bridge (foundation)

**Goal:** Define versioned **control + ring buffer (or double buffer) protocol** between `RawrXD-InferenceEngine` and `RawrXD-Win32IDE`: session id, prompt hash, cancel, heartbeat, error codes, and **variable-length token / logit chunks**.

**Deliverables**

- Header-only or small shared `ipc/` (or `include/sovereign_ipc.hpp`) describing layout, magic, version.
- Headless: open/create mapping, wait/signal (event or slim reader/writer lock pattern), write chunks.
- IDE: read chunks, non-blocking poll from UI thread timer/worker; **no blocking wait on UI thread**.

**Acceptance**

- Round-trip **ping** < 1 ms local (same machine, empty payload).
- **480p-sized** dummy payload burst without IDE input lag (manual or harness).
- Documented **size limits** and behavior on writer crash (stale generation counter → reset).

**BOM:** two processes built from targets above; optional tiny `ipc_smoke` EXCLUDE_FROM_ALL test executable (future).

---

### M1 — Token-at-a-time streaming over IPC

**Goal:** After model load, run decode **one token (or small chunk)** per step and publish to IPC until EOS or cancel.

**Acceptance**

- Cancel mid-stream: engine stops within bounded time; no deadlock.
- Deterministic **session** boundary: two concurrent sessions queued or rejected per documented policy.

**BOM:** `RawrXD-InferenceEngine` + existing loader stack; unit tests for stream state machine where logic is pure.

---

### M2 — Model load / unload and session manager (headless)

**Goal:** Robust lifecycle: load path, OOM handling, unload, **N = 4** sessions serialized or limited without leaking mappings.

**Acceptance**

- Repeated load/unload loop (smoke) passes without handle leaks (tooling: handle count or manual checklist).
- Startup time documented for **tiny** CI model on CI runner; **Mixtral-class** on dev machine.

**BOM:** `model_loader` / mmap paths already in tree; extend with session table in inference service only.

---

### M3 — Ghost text (Win32) + accept

**Goal:** **Sovereign Ghost Text** in `RawrXD-Win32IDE`: non-committed suggestion string, theme colors (Iris / muted grey per Rose Pine), **Tab** commits to buffer.

**Acceptance**

- **Trigger:** idle **200 ms** after last key (configurable).
- **TTFT:** < **400 ms** on reference hardware (document GPU/RAM); measure with log timestamps.
- **Visual:** ghost string drawn without stealing focus; **Tab** inserts text; Esc clears.
- Undo: first version may be single-level undo for last ghost commit; document limitation.

**BOM:** `RawrXD-Win32IDE`; GDI/D2D path already in project — extend minimal overlay layer.

---

### M4 — Swarm + pack cache telemetry (already advanced in tree)

**Goal:** Keep heatmap / EMA / pack hit-miss wired; ensure **staging or local** logs can prove `groupedFallbacks` trend.

**Acceptance**

- Per-layer pack hit/miss visible in logs or snapshot JSON (align with `schemas/expert_heatmap_snapshot.schema.json` if used).

**BOM:** existing tests `test_moe_plan_row_mixture_pack_cache`, swarm scheduler; staging job optional.

---

### M5 — Feature-gated grouped math canary

**Goal:** Grouped down-project (and related fused path) behind **runtime flag**; fallback on pack miss; numerical parity.

**Acceptance**

- `test_moe_mixture_reference` (and numerical gate workflow) pass: **max_abs_diff < 5e-4** (or stricter if tightened).
- Microbench shows **≥ 30%** win on targeted layers when flag on (document machine + model).
- No regression in eviction / pin-block metrics per your stress checklist.

**BOM:** `moe-numerical-gate.yml`, `Run-MoeGroupedGemmSweep.ps1` / `PostProcess-MoeGroupedGemmSweep.ps1`.

---

### M6 — Latency regression gate + production hardening

**Goal:** Add `latency-regression-gate.yml`; baseline CSV/JSON from main; PR compares. Hardening: security on IPC ACL, single-user default.

**Acceptance**

- CI fails when **> 15%** regression vs baseline on agreed metric (e.g. `avg_us_gemm`).

**BOM:** new workflow + committed baseline artifact or generated in CI from pinned runner label (document variance risk).

---

## CI acceptance checklist (minimal)

- [ ] Functional: streaming / IPC smoke (when `ipc_smoke` or equivalent exists).
- [ ] Numerical: `MoE numerical gate` workflow green on main.
- [ ] Perf: local TTFT/TPOT logs attached to release notes for M3+.
- [ ] Safety: stress run — no sustained growth in handles; eviction/pin metrics stable (M5+).
- [ ] Observability: heatmap / pack telemetry export path documented (M4).

---

## Suggested execution order (solo)

1. **M0 Shared Memory Bridge** (this unlocks everything else without UI rewrites).
2. **M1** streaming tokens.
3. **M3** ghost text (parallel light design work OK; integrate after M1).
4. **M2** session hardening (can overlap M3).
5. **M5** grouped math canary when dogfood stable.
6. **M6** latency gate + hardening.

---

## References in-repo

- MoE CI: `.github/workflows/moe-numerical-gate.yml`
- Sweeps: `scripts/Run-MoeGroupedGemmSweep.ps1`, `scripts/PostProcess-MoeGroupedGemmSweep.ps1`
- Pack cache tests: `tests/test_moe_plan_row_mixture_pack_cache.cpp`, `tests/test_moe_mixture_reference.cpp`

---

*Last updated: 2026-04-02*
