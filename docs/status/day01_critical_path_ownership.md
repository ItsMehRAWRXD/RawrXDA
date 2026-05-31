# Day 1 Critical Path Ownership

Date: 2026-04-21
Gate: Day 1 - Baseline Integrity

## Critical Path Ownership

| Work Area | Owner | Blocking If Failed | Primary Evidence |
| --- | --- | --- | --- |
| Workflow persistence and state replay | AgentPolish subsystem | Yes | src/execution_state_persistence.cpp |
| Memory retrieval relevance and safety | AgentPolish subsystem | Yes | src/enhanced_memory_retrieval.cpp |
| Extension host isolation and IPC boundary | ExtensionHost subsystem | Yes | src/win32app/IPC_Channel.cpp |
| Extension sandbox and trust boundaries | ExtensionHost subsystem | Yes | src/win32app/ExtensionSandboxManager.cpp |
| Workspace rename and global symbol operations | LSPComplete subsystem | Yes | src/lsp/crossfile_rename_engine.cpp |
| Runtime performance and stream throughput | Performance subsystem | Yes | src/ai/speculative_decoder.cpp |

## Ownership Acceptance Rules

- Every critical-path row must have a named owner.
- No row can be marked TBD, UNASSIGNED, or N/A.
- Evidence file paths must resolve inside the repository.

## Dependency Order

1. AgentPolish state and memory foundations
2. ExtensionHost isolation and sandboxing
3. LSPComplete workspace-wide correctness
4. Performance throughput and streamability envelope

## Escalation

- Any critical-path row without owner assignment is an immediate Day 1 blocked verdict.
- Any missing evidence anchor requires same-day correction before phase advancement.
