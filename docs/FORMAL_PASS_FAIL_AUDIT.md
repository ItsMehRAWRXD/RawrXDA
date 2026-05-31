# RawrXD IDE: Formal Pass/Fail Audit Sheet

Status: STABILIZATION SPRINT
Audit Date: 2026-05-09
Current Branch: feature/phaseD-flashattention-integration

Primary Evidence Links:
- [docs/PRODUCTION_GATE_AUDIT.md](docs/PRODUCTION_GATE_AUDIT.md)
- [src/core/crash_containment.cpp](src/core/crash_containment.cpp)
- [src/telemetry/crash_handler.cpp](src/telemetry/crash_handler.cpp)
- [src/win32app/TitanIPC.cpp](src/win32app/TitanIPC.cpp)
- [docs/COMPETITIVE_STANDARD_CURSOR_VSCODE_COPILOT.md](docs/COMPETITIVE_STANDARD_CURSOR_VSCODE_COPILOT.md)

Runtime Evidence Paths:
- D:/rxdn_ninja/bin/crash_dumps/crash_manifest.txt
- D:/rxdn_ninja/bin/crash_dumps/

## 1. Fundamental Operational Gates

| Gate | Criteria | Status | Evidence/Blocker |
| --- | --- | --- | --- |
| Diagnostic Integrity | Crash dumps must contain actionable stack traces (Length > 0). | FAIL | 12 consecutive 0-byte .dmp files in D:/rxdn_ninja/bin/crash_dumps/. See [docs/PRODUCTION_GATE_AUDIT.md](docs/PRODUCTION_GATE_AUDIT.md). |
| Startup Stability | IDE must reach a fully idle, interactive state without a fatal AV. | FAIL | Recurrent 0xC0000005 in ntdll.dll at 0x00007FFE8EDC0CB7. See [docs/PRODUCTION_GATE_AUDIT.md](docs/PRODUCTION_GATE_AUDIT.md). |
| SEH Containment | Background worker exceptions must be trapped without process death. | PARTIAL | SEH wrappers exist in [src/win32app/main_win32.cpp](src/win32app/main_win32.cpp), but startup still ends in fatal AV under heavy init. |
| Resource Resilience | Successful 22B GGUF model load without UI thread starvation. | PARTIAL | Model-ready path exists, but stability under heavy-init remains non-compliant. See [src/win32app/Win32IDE_Core.cpp](src/win32app/Win32IDE_Core.cpp). |

## 2. Core Usability and Parity Gates (Finished Bar)

Based on [docs/COMPETITIVE_STANDARD_CURSOR_VSCODE_COPILOT.md](docs/COMPETITIVE_STANDARD_CURSOR_VSCODE_COPILOT.md)

| Feature Area | Finished Capability Requirement | Current Reality |
| --- | --- | --- |
| Ghost Text | Sub-300ms inline AI suggestions with multi-line support. | Gap: subsystem exists, but wiring/parity remains open debt. Evidence: [src/win32app/Win32IDE_GhostText.cpp](src/win32app/Win32IDE_GhostText.cpp), [docs/GAP_VS_CURSOR_VSCODE_COPILOT.md](docs/GAP_VS_CURSOR_VSCODE_COPILOT.md). |
| LSP Diagnostics | Real-time squiggles and high-fidelity hover tooltips. | Gap: declared-vs-real drift remains unresolved; parity not yet validated in sustained runtime. Evidence: [src/lsp/RawrXD_LSPServer.cpp](src/lsp/RawrXD_LSPServer.cpp). |
| Terminal (PTY) | ConPTY/Unix PTY with VT100 support and zero-lag scrollback. | Partial: architecture present, but sustained-session runtime proof is not yet complete under current stability failures. Evidence: [src/core/pty_terminal.hpp](src/core/pty_terminal.hpp). |
| Debug (DAP) | Breakpoints, stack-frame navigation, and variable inspection. | Gap: full DAP-grade UX remains a known deficiency. Evidence: [src/core/debug_adapter.hpp](src/core/debug_adapter.hpp), [docs/GAP_VS_CURSOR_VSCODE_COPILOT.md](docs/GAP_VS_CURSOR_VSCODE_COPILOT.md). |
| Git Integration | Integrated diffing, staging, and branch management. | Gap: partial tooling only; production-grade end-to-end UX flow not yet proven. Evidence: [src/win32app/Win32IDE_GitPanel.cpp](src/win32app/Win32IDE_GitPanel.cpp). |

## 3. Immediate Action Requirements to Clear Audit

1. Fix dump generation first.
Action: instrument crash dump write path to log GetLastError/HRESULT from MiniDumpWriteDump and persist it in crash logs.
Evidence targets: [src/core/crash_containment.cpp](src/core/crash_containment.cpp), [src/telemetry/crash_handler.cpp](src/telemetry/crash_handler.cpp).

2. Resolve ntdll AV second.
Action: once dump is non-zero and actionable, identify first non-ntdll frame and patch underlying corruption.
Evidence target: [docs/PRODUCTION_GATE_AUDIT.md](docs/PRODUCTION_GATE_AUDIT.md).

3. Harden heavy-init and manifest truthfulness.
Action: remove or downgrade aspirational entries where runtime proof is missing to reduce declared-vs-real drift.
Evidence target: [src/win32app/Win32IDE_FeatureManifest.cpp](src/win32app/Win32IDE_FeatureManifest.cpp).

4. Verify sustained clean run.
Action: run Launch -> Edit -> 22B Load -> Chat -> Exit with zero crash entries.
Evidence target: D:/rxdn_ninja/bin/crash_dumps/crash_manifest.txt.

## 4. Titan Runtime Findings (Operational Interpretation)

1. Confirmed issue class: deployment/runtime mismatch, not solely core-IDE architectural collapse.
Evidence: debug CRT dependency observed in Titan host lane and mitigated by release-safe host preference plus improved launcher diagnostics in [src/win32app/TitanIPC.cpp](src/win32app/TitanIPC.cpp).

2. Confirmed high-impact behavior chain when wrong lane is selected:
silent host launch failure, immediate host termination, IPC timeout, chat send failure, watchdog-triggered shutdown, and secondary fallback instability.

3. Confirmed observability improvement:
launcher now records attempted paths and last Win32 launch error, materially improving triage velocity.

4. Critical unknown still open:
Is recurring ntdll 0xC0000005 primarily secondary fallout from host/runtime mismatch, or independent core memory corruption?
Status: unresolved until GATE-RT-02 is PASS and first actionable stack proves primary fault origin.

## 5. Build Integrity Enforcement Gates (New)

| Gate | Criteria | Status | Evidence/Blocker |
| --- | --- | --- | --- |
| Lane Segregation | Debug, release, and experimental artifacts must be isolated by output root and non-overlapping launch candidates. | FAIL | Multiple build roots and candidate probing behavior indicate lane contamination risk. Evidence: [src/win32app/TitanIPC.cpp](src/win32app/TitanIPC.cpp). |
| CRT Compliance | Shipping helper processes must not import debug CRT dependencies. | PARTIAL | Titan release-safe lane is preferred, but enforcement is not yet globally codified across all helper processes. |
| Startup Artifact Verification | Pre-launch checks must validate selected helper binary path, architecture, and dependency lane before use. | FAIL | Runtime diagnostics improved, but hard pre-launch validation gate is not yet mandatory for all subprocesses. |
| Host ABI Compatibility | Parent and helper process contracts must verify version/ABI compatibility before request traffic. | FAIL | No global ABI handshake gate is recorded as enforced for all helper lanes. |
| Stale Artifact Rejection | Startup path must reject stale/duplicate binaries that shadow current lane outputs. | FAIL | Candidate fallback still depends on filesystem state in mixed-lane roots; deterministic stale rejection policy not yet formalized. |

Operational note:
These gates are mandatory because one mis-laned helper binary can destabilize the entire IDE runtime ecosystem (IPC, plugin host, PTY worker, and agent subprocesses).

## 6. Gate Dependency Graph (Critical Path)

| Gate | Depends On |
| --- | --- |
| RT-03 Sustained 60-minute session | RT-01 Startup Stability |
| USE-01 Launch -> Open -> Edit -> Save | RT-01 Startup Stability |
| USE-05 Debug start + breakpoint + variable inspect | FI-03 Substrate linkage validation |
| PAR-03 Debug step-over/step-in/continue parity | USE-05 Debug start + breakpoint + variable inspect |
| PAR-01 Inline completion p95 target | RT-01 Startup Stability + USE-03 Ghost Text runtime validation |
| FI-01 Command-surface verification | RT-01 Startup Stability |
| FI-02 Manifest truthfulness (drift <= threshold) | FI-01 Command-surface verification |

Root unlock node:
RT-02 Diagnostic Integrity (non-zero actionable dumps) is the primary unlock for reliable root-cause closure on RT-01.

## 7. Additional Runtime Gate (New)

| Gate | Criteria | Status | Evidence/Blocker |
| --- | --- | --- | --- |
| RT-04 Heap Integrity | PageHeap clean, Application Verifier clean, no invalid handle usage, and no post-free access under stress harness. | FAIL | Gate not yet instrumented as an enforced pre-ship validation lane; mixed async/UI/IPC/subprocess topology increases memory-lifetime risk until proven clean. |

RT-04 is required because concurrent PTY, DAP, AI streaming, extension host, and async UI paths elevate the probability and impact of latent heap/lifetime bugs.

## 8. Strategic Priority Stack

| Priority | Focus |
| --- | --- |
| P0 | Dump pipeline reliability (RT-02) |
| P0 | Runtime lane integrity (Build Integrity gates) |
| P0 | Startup crash elimination (RT-01) |
| P1 | Substrate linkage validation (FI-03) |
| P1 | Sustained-session stability (RT-03, USE-01) |
| P2 | Command-surface verification (FI-01) |
| P2 | Manifest truthfulness (FI-02) |
| P3 | UX parity tuning (USE/PAR gates) |
| P4 | Net-new feature expansion |

## Audit Verdict

UNSTABLE

The command surface breadth is high, but operational reliability under heavy-load startup remains below finished-state requirements until Diagnostic Integrity and Startup Stability gates are both PASS.
