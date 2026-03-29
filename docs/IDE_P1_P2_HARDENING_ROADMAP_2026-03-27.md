# RawrXD IDE P1/P2 Hardening Roadmap (2026-03-27)

## Status Snapshot

- P0 is complete:
  - DAP foundation is in place
  - extension host hardening baseline is complete
  - SCM panel baseline is complete
  - minimal settings service is complete
- Current objective: move from production-ready core to hardened IDE platform.

## Planning Principles

- Prioritize user-visible reliability over feature count.
- Finish thin vertical slices with validation, not broad partial wiring.
- Reuse existing Win32IDE surfaces before adding new abstractions.
- Require acceptance tests for every hardening item.

## Phase Overview

### P1: Hardening and UX Parity

Goal: make the existing IDE surfaces resilient, testable, and operationally coherent.

1. P1.1 DAP Integration
2. P1.2 LSP Hardening
3. P1.3 Terminal ConPTY Hardening
4. P1.4 Process Isolation for Extension Host
5. P1.5 SCM Expansion

### P2: Platform Completion and Differentiation

Goal: close remaining parity gaps and add RawrXD-specific leverage where it matters.

1. P2.1 Keybinding Resolver and Persistence
2. P2.2 Workbench Layout Persistence
3. P2.3 Extension Marketplace Production Cleanup
4. P2.4 MASM Project Experience
5. P2.5 Privacy, Telemetry, and Policy Controls

## Recommended Execution Order

1. P1.1 DAP Integration
2. P1.3 Terminal ConPTY Hardening
3. P1.2 LSP Hardening
4. P1.5 SCM Expansion
5. P1.4 Process Isolation
6. P2.1 Keybinding Resolver and Persistence
7. P2.2 Workbench Layout Persistence
8. P2.3 Extension Marketplace Production Cleanup
9. P2.4 MASM Project Experience
10. P2.5 Privacy, Telemetry, and Policy Controls

Rationale:
- DAP is the highest-value remaining parity gap and already has a service foundation.
- Terminal reliability affects multiple workflows and should be stabilized before broader UX expansion.
- LSP hardening benefits from the same lifecycle and recovery patterns needed elsewhere.
- SCM and process-isolation work are easier to validate once debugging, terminal, and transport behavior are stable.

## P1 Detail

### P1.1 DAP Integration

Objective:
- connect `DapClientService` to the active Win32IDE debugger flow and make debug sessions usable end-to-end.

Implementation scope:
- wire launch and attach flows from debugger UI into `src/core/dap_client_service.*`
- map breakpoints from editor/debugger state into DAP `setBreakpoints`
- handle core events: initialized, stopped, continued, exited, terminated, output, thread
- populate stack frames, scopes, variables, and evaluate requests in the debugger UI
- persist breakpoints across session restart using the new settings/configuration path where appropriate

Primary code areas:
- `src/core/dap_client_service.h`
- `src/core/dap_client_service.cpp`
- `src/win32app/Win32IDE_Debugger.cpp`
- any existing debug panel or session state files already used by Win32IDE

Acceptance criteria:
- launch works with at least one native adapter
- attach works where supported by the adapter under test
- breakpoint hit updates UI correctly
- step over, step in, continue, and stop all behave correctly
- stack trace and variable inspection render in the debugger panel
- evaluate works in the current frame for at least one adapter

Validation:
- scripted smoke tests against at least two adapters
- manual UI validation for launch, breakpoint, step, inspect, terminate

Estimated effort:
- 2 to 3 focused sessions

### P1.2 LSP Hardening

Objective:
- make language-server sessions resilient across crashes, file churn, and multi-language editing.

Implementation scope:
- add restart and backoff behavior for failed server processes
- ensure clean shutdown and reinitialize on workspace or document state change
- support multiple active servers with diagnostics fan-in
- improve capability handling for servers with partial feature support
- harden document open/change/close synchronization

Primary code areas:
- `src/lsp_client.h`
- `src/lsp_client.cpp`
- `src/agentic_ide.cpp`
- editor integration points that consume diagnostics and navigation

Acceptance criteria:
- server crash does not require IDE restart
- diagnostics recover after restart
- completion/definition continue to work after reconnect
- two different language servers can run without cross-contaminating state

Validation:
- restart fault injection
- open/edit/save/close smoke tests across at least two languages

Estimated effort:
- 2 to 3 focused sessions

### P1.3 Terminal ConPTY Hardening

Objective:
- move terminal reliability from fallback-grade to production-grade on Windows.

Implementation scope:
- verify ConPTY-first usage where supported
- validate resize propagation and rendering fidelity
- harden unicode, long-output, and cancellation behavior
- ensure scrollback and session cleanup behave deterministically
- document fallback behavior when ConPTY is unavailable

Primary code areas:
- `src/terminal_pool.h`
- `src/terminal_pool.cpp`
- Win32IDE terminal UI integration files

Acceptance criteria:
- shell resize updates correctly
- unicode output renders without corruption
- long-running commands remain responsive
- cancellation and shell exit clean up handles and UI state

Validation:
- scripted resize/output tests
- PowerShell and cmd smoke runs

Estimated effort:
- 1 to 2 focused sessions

### P1.4 Process Isolation for Extension Host

Objective:
- ensure extension faults cannot destabilize the IDE process.

Implementation scope:
- move extension execution into a dedicated host process boundary
- define IPC contract for command execution, activation, and lifecycle events
- add crash detection and restart policy
- harden path validation and install/update/uninstall transitions around the new process model

Primary code areas:
- extension host implementation files
- marketplace/extension manager paths
- process and IPC utilities already present in the codebase

Acceptance criteria:
- extension host crash does not crash Win32IDE
- host restart is surfaced clearly to the user
- install, enable, disable, uninstall continue to function

Validation:
- synthetic extension host crash testing
- offline and online extension lifecycle smoke tests

Estimated effort:
- 4 to 6 focused sessions

### P1.5 SCM Expansion

Objective:
- extend the current SCM baseline into an efficient daily-use workflow.

Implementation scope:
- add diff viewer integration for staged and unstaged content
- add per-file context actions for stage, unstage, discard, open diff
- add commit validation and clearer status/error handling
- improve branch actions where already supported by backend logic

Primary code areas:
- existing Win32IDE SCM/sidebar files
- git tool handlers already used for status and staging

Acceptance criteria:
- user can inspect diffs without leaving the IDE
- per-file stage and unstage actions behave correctly
- invalid commit attempts produce actionable errors

Validation:
- repo and non-repo smoke tests
- staged/unstaged/merge-conflict scenarios

Estimated effort:
- 3 to 4 focused sessions

## P2 Detail

### P2.1 Keybinding Resolver and Persistence

Objective:
- introduce a unified, context-aware keybinding system with user overrides.

Scope:
- centralized keybinding registry and lookup
- context evaluation for editor, terminal, sidebar, debugger, and dialogs
- user override persistence using configuration service
- conflict detection and basic resolution UX

Exit gate:
- user-defined overrides persist and reload cleanly

### P2.2 Workbench Layout Persistence

Objective:
- preserve window, panel, sidebar, and split-editor state consistently.

Scope:
- persist docking and visibility state
- restore layout on launch
- stabilize split-editor and panel transitions

Exit gate:
- layout survives restart without broken pane state

### P2.3 Extension Marketplace Production Cleanup

Objective:
- remove transitional implementation debt and bring marketplace flows up to production expectations.

Scope:
- replace placeholder or inconsistent manager paths
- tighten install/update/remove behavior
- improve error reporting and rollback handling

Exit gate:
- extension lifecycle is stable in both offline and online scenarios

### P2.4 MASM Project Experience

Objective:
- lean into RawrXD’s reverse-engineering and MASM strengths.

Scope:
- MASM project templates and starter layout
- assembler path onboarding and validation
- starter build/debug presets for Win32 and x64 assembly workflows

Exit gate:
- new MASM project can be created, built, and debug-launched from the IDE

### P2.5 Privacy, Telemetry, and Policy Controls

Objective:
- make telemetry and privacy behavior explicit, controllable, and reviewable.

Scope:
- policy modes for local-only, consented telemetry, and enterprise-restricted operation
- UI for privacy choices and current state
- documented defaults and logging behavior

Exit gate:
- telemetry mode is visible, persisted, and enforced consistently

## Cross-Cutting Requirements

Every item in P1 and P2 should include:
- structured logging for failure analysis
- explicit user-facing error states
- at least one scripted smoke test path
- no regressions in non-repository or no-workspace scenarios
- settings persistence routed through the canonical configuration service when applicable

## Suggested Milestones

### Milestone M1

- P1.1 complete
- P1.3 complete

Outcome:
- debugging and terminal workflows become viable for daily use.

### Milestone M2

- P1.2 complete
- P1.5 complete

Outcome:
- core editing and source-control flows become operationally dependable.

### Milestone M3

- P1.4 complete
- P2.1 complete
- P2.2 complete

Outcome:
- platform architecture and user customization semantics become coherent.

### Milestone M4

- P2.3 complete
- P2.4 complete
- P2.5 complete

Outcome:
- marketplace and RawrXD-specific differentiation are ready for broader release positioning.

## Definition of Done

The IDE can be considered hardened when:
- debug sessions are stable across adapters and restart cycles
- terminal sessions are ConPTY-reliable and unicode-safe
- language servers recover without manual IDE restart
- SCM supports inspect/stage/unstage/commit workflows cleanly
- extension crashes are isolated from the IDE process
- user customization systems behave predictably across restart

## Immediate Next Step

Start P1.1 DAP Integration.

Reason:
- it is the highest-value remaining parity gap
- the core service is already implemented
- its acceptance criteria are concrete and testable
- finishing it will reduce uncertainty for later debugger, settings, and workbench work