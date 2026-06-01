# Sovereign Inference IDE - Frontend v1.1-alpha

## Overview

This is the **Day 1 implementation** of the Sovereign Inference IDE frontend. It implements the **Status & Health Dashboard**, which polls the inference engine at 1Hz and visualizes engine state changes in real-time.

## Architecture

```
frontend/
├── src/
│   ├── engine/
│   │   ├── EngineService.ts       # Core polling service (framework-agnostic)
│   │   └── StatusMapper.ts        # State-to-UI mapping logic
│   ├── components/
│   │   ├── StatusDashboard.tsx    # Main dashboard component
│   │   └── StatusDashboard.css    # Dashboard styling
│   ├── App.tsx                    # Main app layout
│   ├── App.css                    # Layout styling
│   └── main.tsx                   # React entry point
├── index.html                     # HTML entry point
├── vite.config.ts                 # Vite build config
├── tsconfig.json                  # TypeScript config
└── package.json                   # Dependencies

```

## Governance Principles

**Engine-as-Authority:**
- ✅ UI reads `state`, `suggested_action`, `can_retry`, `fault_class` from `/status` endpoint
- ✅ UI disables/enables controls based on engine policy
- ❌ No retry timers in frontend code
- ❌ No fault-guessing logic
- ❌ No model-selection logic (engine recommends)

## Quick Start

### 1. Install Dependencies

```bash
cd d:\rawrxd-ci-bootstrap\frontend
npm install
```

### 2. Start the Backend Engine

In a separate terminal:

```bash
cd d:\rawrxd-ci-bootstrap
_build_ide_integration.cmd
```

This compiles and runs the engine on `http://localhost:11435`.

### 3. Start the Frontend Dev Server

```bash
cd d:\rawrxd-ci-bootstrap\frontend
npm run dev
```

The frontend will start on `http://localhost:5173`.

### 4. Open in Browser

Navigate to `http://localhost:5173` and watch the Status Dashboard poll the engine.

**Expected behavior:**
- Dashboard shows "Connecting..." briefly
- Changes to "Idle" (Gray ⚪) if engine is idle
- Changes to "Loading" (Blue ⏳) if engine is loading a model
- Changes to "Ready" (Green 🟢) if engine is ready for inference
- Changes to "Fault" (Red ⚠️) if engine encounters an error (displays fault panel)

## Key Components

### EngineService.ts

Framework-agnostic service that:
- Polls `/status` endpoint at 1Hz
- Manages subscription callbacks
- Caches last status for queries
- Provides `isReady()`, `isFaulted()`, `getSuggestedAction()` helpers
- Exports singleton instance `engineService`

### StatusDashboard.tsx

React component that:
- Subscribes to EngineService status updates
- Maps engine state (0-3) to UI primitives
- Renders fault panel when `state === 3` (shows policy from engine)
- Renders ready panel when `state === 2` (shows recommended model)
- Follows Engine-as-Authority governance

### StatusMapper.ts

Pure function utility that maps engine state codes to UI styles:
- `0` → "Idle" (Gray)
- `1` → "Loading" (Blue)
- `2` → "Ready" (Green)
- `3` → "Fault" (Red)

## CI/CD Integration

This frontend is part of the larger `PROJECT_PLAN.md` sprint. On **Day 10**, the CI/CD gate (`ci_gate.ps1`) will:

1. Build the engine binary
2. Run the handshake test
3. Run this frontend as a validation probe
4. Package the alpha release

## Next Steps (Days 2-10)

- **Days 3-4:** Add Inference Loop (streaming tokens)
- **Day 5:** Add Sidecar Resilience (read `headless_fault_policy.json`)
- **Days 6-8:** Add Benchmarking & Model Selector
- **Day 9:** Add Safety Guardrails
- **Day 10:** Run full CI/CD gate

---

**Status:** ✅ Day 1 Complete
**Last Updated:** June 1, 2026
