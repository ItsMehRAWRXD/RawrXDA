# RawrXD IDE Finalization Gap Analysis (2026-03-27)

## Scope
This audit focuses on missing work between the current RawrXD state and a production-finalized IDE comparable to mainstream AI IDE baselines.

## Method
Source-backed verification across core IDE surfaces:
- process model
- extension/marketplace
- LSP
- debugging
- terminal
- git UX
- settings/keybindings
- UI workbench composition

## Executive Verdict
- Agentic zero-wrapper core: production-ready
- Full IDE platform parity: not yet complete
- Primary blockers: debugging protocol stack, extension host hardening, and first-class IDE UX surfaces (SCM panel, robust settings/keybinding service)

## Evidence-Backed Status Matrix

### 1) Process Architecture
Status: PARTIAL

What exists:
- Explicit process spawning for tools/commands via CreateProcess in multiple components.
- Significant async/threaded orchestration.

Evidence:
- src/agentic/AgentToolHandlers.cpp
- src/agentic/agentic_tool_executor.cpp
- src/agentic_engine.cpp
- src/api_server.cpp

Gap:
- No clear VS Code-style dedicated extension-host process boundary with strict crash/isolation semantics for third-party code.
- Current model appears hybrid (single IDE process + subprocess launches), not a formal multi-process IDE architecture.

### 2) Extension / Marketplace System
Status: PARTIAL (mixed maturity)

What exists:
- VSIX/marketplace command paths and extension marketplace components.
- CLI extension commands and marketplace query/install command surfaces.

Evidence:
- src/cli/cli_extension_commands.cpp
- src/marketplace/extension_marketplace.cpp
- src/marketplace/vscode_marketplace.cpp
- src/core/command_registry.hpp (marketplace command range)

Gap:
- Presence of transitional/placeholder code in some marketplace manager paths indicates uneven production readiness.

Evidence of concern:
- src/marketplace/extension_marketplace_manager.cpp (contains non-production placeholder constructs and inconsistent native conversion artifacts)
- src/core/auto_feature_stub_impl.cpp (marketplace stubs)

### 3) LSP Integration
Status: PARTIAL-TO-STRONG

What exists:
- Concrete LSP client transport over stdio with JSON-RPC framing.
- LSP methods wired: initialize, didOpen, didChange, completion, definition.
- IDE wiring points reference LSP client integration.

Evidence:
- src/lsp_client.h
- src/lsp_client.cpp
- src/agentic_ide.cpp
- src/agentic_text_edit.h

Gap:
- Fallback behavior and in-memory transport are present; robust multi-server lifecycle, capability negotiation breadth, and diagnostics fan-in need hardening/acceptance tests for production parity.

### 4) Debugging (DAP)
Status: PARTIAL (foundation now present)

What exists:
- Existing debugger surfaces and legacy DAP-related files.
- New compiled DAP foundation service with stdio JSON-RPC framing and core request flow.

Evidence:
- src/win32app/Win32IDE_Debugger.cpp
- src/features/dap_debugger_full.cpp
- src/core/dap_client_service.h
- src/core/dap_client_service.cpp
- CMakeLists.txt (Win32IDE sources include src/core/dap_client_service.cpp)

Gap:
- Needs full UI/session integration and adapter-specific hardening (multi-thread events, robust reconnection, breakpoint persistence, adapter matrix validation).

### 5) Integrated Terminal
Status: PARTIAL

What exists:
- TerminalPool implementation with process-backed shell IO.
- Agentic command routing to terminal-related flows.

Evidence:
- src/terminal_pool.h
- src/terminal_pool.cpp
- src/agentic_ide.cpp
- src/agentic/AgenticNavigator.cpp

Gap:
- Current terminal backend is a fallback process+pipe model; no mature ConPTY-first implementation quality gates visible (resize fidelity, shell integration, robust session persistence).

### 6) Git Integration
Status: PARTIAL

What exists:
- Tool-level git operations including status/add/commit/push in backend/agentic layers.

Evidence:
- src/agentic/AgentToolHandlers.cpp (git_status)
- src/backend/agentic_tools.cpp

Gap:
- No confirmed full IDE SCM panel parity (staging UI, inline diff workflow, branch UX comparable to mature IDEs).

### 7) Settings / Configuration System
Status: PARTIAL

What exists:
- Multiple config loaders and JSON-based settings paths in subsystem modules.

Evidence:
- src/agentic_configuration.cpp
- src/agentic_configuration_qt_free.cpp
- src/cloud_settings_dialog.h

Gap:
- No single canonical, hierarchical configuration service equivalent (user/workspace/folder scope, dynamic update contract) clearly enforced across the IDE.

### 8) Keybinding System
Status: PARTIAL

What exists:
- Shortcut metadata and accelerator hooks in feature registry and bridge paths.

Evidence:
- src/feature_registry_panel.cpp
- src/feature_registry_panel.h
- src/agentic/bridge/Win32IDEBridge.cpp

Gap:
- No unified context-aware keybinding resolver with conflict resolution and user customization persistence at IDE scale.

### 9) Workbench/UI Composition
Status: PARTIAL

What exists:
- Win32IDE bridge/nav concepts and named UI regions (terminal/panels/sidebar references).

Evidence:
- src/agentic/bridge/Win32IDEBridge.cpp
- src/agentic/AgenticCopilotIntegration.cpp

Gap:
- Need explicit layout service guarantees for docking, split editors, and persisted window/workbench layout behaviors.

## Competitive Delta (Practical)

Closest-to-ready:
- agentic execution/tooling layer
- baseline LSP plumbing

Largest remaining parity gaps versus mainstream IDEs:
1. hardened extension-host architecture and marketplace flow quality
2. first-class SCM/workbench/settings/keybinding UX platformization
3. DAP production hardening and integration completion

## Finalization Plan (Priority)

### P0 (must ship for production IDE parity)
1. Extension host hardening
   - isolate extension runtime process boundary
   - stabilize marketplace/VSIX manager path; remove/replace transitional placeholders
2. Terminal hardening
   - ConPTY-first backend with resize/session reliability
3. Unified configuration service
   - user/workspace scopes + live reload + schema contract

### P1 (high-value UX parity)
4. DAP production hardening + workbench/debug panel integration
5. SCM panel and diff UX
6. Context-aware keybinding resolver with persistence
7. Workbench layout persistence and split/dock guarantees

### P2 (differentiation)
8. MASM-specific project wizards/resources/toolchain onboarding
9. Enterprise telemetry/privacy controls with explicit policy modes

## Exit Criteria for “Production-Finalized IDE”

- DAP: pass scripted breakpoint/step/inspect test suite on at least 2 language adapters
- Extension host: extension crash does not crash IDE; install/update/uninstall passes offline+online scenarios
- Terminal: ConPTY resize, cancellation, and long-running command reliability pass
- SCM: stage/unstage/commit/branch switch UI flow pass
- Settings/keybindings: user override persistence and reload pass

## Risk Assessment

- Functional core risk: low (agentic core is strong)
- Platform parity risk: medium-high until DAP + extension host + UX platform services are completed

## Recommended Immediate Next Task

Complete DAP integration hardening: connect `DapClientService` to existing debugger UI flow, then validate against at least two adapters with scripted smoke tests for launch/attach, breakpoints, stack trace, and evaluate.
