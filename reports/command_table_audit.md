# COMMAND_TABLE Coverage Audit Report
Generated: 2026-02-11 07:46:36
Source root: `D:\rawrxd\src`

## Summary

| Metric | Count |
|--------|-------|
| IDM_* defines (non-zero, non-range) | 431 |
| COMMAND_TABLE entries | 472 |
| Handler declarations | 258 |
| GUI-routable (ID ≠ 0) | 446 |
| CLI-accessible (BOTH + CLI_ONLY) | 458 |
| **Coverage** | **100.0%** |
| Status | ✅ CLEAN |

## Categories

| Category | Commands |
|----------|----------|
| Debug | 28 |
| ReverseEng | 26 |
| CLI | 26 |
| Swarm | 25 |
| Edit | 23 |
| Router | 21 |
| File | 20 |
| Plugin | 19 |
| View | 17 |
| Theme | 17 |
| Hotpatch | 17 |
| Voice | 17 |
| AIMode | 13 |
| LSP | 13 |
| ASM | 12 |
| Hybrid | 12 |
| MultiResp | 12 |
| QW | 12 |
| Terminal | 11 |
| Agent | 11 |
| Backend | 11 |
| Tools | 10 |
| Transparency | 9 |
| LSPServer | 9 |
| PDB | 9 |
| AIContext | 7 |
| Audit | 7 |
| Help | 6 |
| Autonomy | 6 |
| Monaco | 6 |
| Telemetry | 6 |
| Git | 5 |
| SubAgent | 5 |
| Editor | 5 |
| Governor | 4 |
| Safety | 4 |
| Replay | 4 |
| Modules | 3 |
| Confidence | 2 |
| Gauntlet | 2 |

## ✅ Full Coverage

All IDM_* defines from Win32IDE.h are represented in COMMAND_TABLE.

## ℹ️ Duplicate CLI Aliases

These may be intentional (legacy aliases).

- **`!decomp_rename`**: `re.decompRename`, `decomp.renameVar`

## ℹ️ Declared Handlers Not in COMMAND_TABLE

These handlers exist in `feature_handlers.h` but aren't
referenced by any COMMAND_TABLE entry. They may be called
directly or are waiting to be wired up.

- `handleAgentGoal`
- `handleBackendList`
- `handleBackendSelect`
- `handleBackendStatus`
- `handleBreakpointAdd`
- `handleBreakpointList`
- `handleBreakpointRemove`
- `handleConfidence`
- `handleDebugContinue`
- `handleDebugStart`
- `handleDebugStep`
- `handleDebugStop`
- `handleExplain`
- `handleGovernor`
- `handleHistory`
- `handleMultiResponse`
- `handlePolicy`
- `handleReplay`
- `handleSafety`
- `handleSwarmDistribute`
- `handleSwarmRebalance`
- `handleTerminalNew`
- `handleTools`
