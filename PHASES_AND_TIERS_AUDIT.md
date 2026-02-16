# RawrXD IDE — Phases and Tiers Audit

**Purpose:** Single reference for all **Phases** and **Tiers** in the Win32 IDE: what is completed (initialized, shut down, and wired to GUI) and what is missing or partial.

**Sources:** Win32IDE_Core.cpp (deferredHeavyInitBody, onDestroy), Win32IDE_Commands.cpp (routeCommandUnified), Win32IDE.h, Win32IDE_FeatureManifest.cpp, Ship docs.

---

## 1. TIERS — Completed

| Tier | Name | Init | Shutdown | Command range | Status |
|------|------|------|----------|---------------|--------|
| **Tier 1** | Critical Cosmetics (smooth scroll, minimap, fuzzy palette, auto-update) | initTier1Cosmetics() | shutdownTier1Cosmetics() | 12000-12100 handleTier1Command | Completed |
| **Tier 3** | Polish (smooth caret, ligatures, file watcher, bracket pairs, zen, fold, lightbulb) | initTier3Polish() | shutdownTier3Polish() | 12100-12200 handleTier3CosmeticsCommand | Completed |
| **Tier 5** | Cosmetic features 40-50 (Line Ending, Network, Test Explorer, Debug Watch, Call Stack, Marketplace, Telemetry Dashboard, Shortcut Editor, Color Picker, Emoji, Crash) | initTier5Cosmetics() | via Cursor parity / app exit | 11500-11600 handleCursorParityCommand | Completed |

---

## 2. TIERS — Missing

| Tier | Notes |
|------|--------|
| **Tier 2** | Not referenced in init, shutdown, or command routing. Missing unless defined elsewhere. |
| **Tier 4** | Not referenced. Missing unless defined elsewhere. |

---

## 3. PHASES — Completed (init + shutdown in Core)

| Phase | Name | Init | Shutdown |
|-------|------|------|----------|
| Phase 6 | Failure Intelligence | initFailureIntelligence() | — |
| Phase 10 | Execution Governor + Safety + Replay + Confidence | initPhase10() | shutdownPhase10() |
| Phase 11 | Distributed Swarm Compilation | initPhase11() | shutdownPhase11() |
| Phase 12 | Native Debugger Engine (DbgEng) | initPhase12() | shutdownPhase12() |
| Phase 18B | Decompiler View (Direct2D) | initDecompilerView() | — |
| Phase 19 | Feature Manifest and Cross-IDE Alignment | compile-time manifest | — |
| Phase 29+36 | VS Code Extension API + QuickJS VSIX Host | initVSCodeExtensionAPI() | shutdownVSCodeExtensionAPI() |
| Phase 32B | Chain-of-Thought Multi-Model Review | initChainOfThought() | — |
| Phase 33 | Voice Chat Engine | initVoiceChat() + panel | shutdownVoiceChat() |
| Phase 33 | Quick-Win Systems (Shortcuts, Backups, Alerts, SLO) | initQuickWinSystems() | shutdownQuickWinSystems() |
| Phase 34 | Telemetry Export Subsystem | initTelemetry() | shutdownTelemetry() |
| Phase 36 | Flight Recorder (binary ring-buffer) | initFlightRecorder() | shutdownFlightRecorder() |
| Phase 36 | MCP Integration (Model Context Protocol) | initMCP() | shutdownMCP() |
| Phase 43 | Plugin System (Native Win32 DLL loading) | initPluginSystem() | shutdownPlugins() |
| Phase 44 | Voice Automation (TTS for AI responses) | Panel created, m_voiceAutomationInitialized | Win32IDE_DestroyVoiceAutomationPanel() |

---

## 4. PHASES — Wired via command handlers (no dedicated initPhase in Core)

| Phase | Name | Command range | Handler |
|-------|------|---------------|---------|
| Phase 13 | Distributed Pipeline Orchestrator | 11000-11100 | handlePipelineCommand |
| Phase 14 | Hotpatch Control Plane | 11100-11200 | handleHotpatchCtrlCommand |
| Phase 15 | Static Analysis Engine | 11200-11300 | handleStaticAnalysisCommand |
| Phase 16 | Semantic Code Intelligence | 11300-11400 | handleSemanticCommand |
| Phase 17 | Enterprise Telemetry and Compliance | 11400-11500 | handleTelemetryCommand |
| Phase 45 | Game Engine Integration (Unity + Unreal) | 10600-10700 | handleGameEngineCommand |
| Phase 48 | The Final Crucible | 10700-10800 | handleCrucibleCommand |
| Phase 49 | Copilot Gap Closer | 10800-10900 | handleCopilotGapCommand |

---

## 5. PHASES — Referenced but not in init sequence

| Phase | Where referenced | Status |
|-------|------------------|--------|
| Phase 31 | Audit menu (Run Full Audit, Check Menu Wiring) | Menu and handlers exist; no initPhase31 (audit on-demand) |
| Phase 32 | Gauntlet (Run All Tests); Phase 32A/32B CoT | Phase 32B CoT has init; Gauntlet menu exists |
| Phase 9 / 9A-9C | ASM Semantic, LSP-AI Bridge, Multi-Response | Multi-Response and LSP have init; ASM semantic may be elsewhere |
| Phase 8B / 8C | Backend Switcher, LLM Router | Backend manager init; router used by agent/backend |

---

## 6. FLAGSHIP / CURSOR PARITY (13000+)

| Block | Name | Command range | Status |
|-------|------|---------------|--------|
| Flagship | Provable Agent, AI Reverse Engineering, Airgapped Enterprise | 13000-13100 | handleFlagshipCommand wired |
| Cursor/JB parity | Tier 5 cosmetic modules | 11500-11600 | handleCursorParityCommand wired |

---

## 7. MISSING OR GAP ITEMS

| Item | Notes |
|------|--------|
| Tier 2 | Not referenced. Missing unless defined elsewhere. |
| Tier 4 | Not referenced. Missing unless defined elsewhere. |
| Phase 1-5 | Qt removal phases (Ship docs); build/script phases, not IDE runtime. N/A. |
| Phase 7-9 | Policy (7), Explainability (8), Backend (8B), Router (8C), ASM (9A), LSP-AI (9B), Multi-Response (9C). Some have init; others command-palette only. |
| Phase 20-28 | Not in Win32IDE_Core init order. Unverified. |
| Phase 37-42 | Not in init sequence. Missing or folded into other phases. |
| Phase 45 Disk Recovery | 10300-10400 handleRecoveryCommand wired. Panel init may be lazy. |
| Phase 46-47 | Not clearly mapped. |

---

## 8. SUMMARY

- **Tiers completed:** Tier 1, Tier 3, Tier 5.
- **Tiers missing:** Tier 2, Tier 4.
- **Phases with init in Core:** 6, 10, 11, 12, 18B, 19, 29+36, 32B, 33 (x2), 34, 36 (x2), 43, 44.
- **Phases with handlers only:** 13, 14, 15, 16, 17, 45, 48, 49.
- **Phases unverified or gap:** 20-28, 37-42, 46-47.
