# COMMAND_TABLE Coverage Audit Report
Generated: 2026-02-11 07:25:14
Source root: `D:\rawrxd\src`

## Summary

| Metric | Count |
|--------|-------|
| IDM_* defines (non-zero, non-range) | 431 |
| COMMAND_TABLE entries | 416 |
| Handler declarations | 258 |
| GUI-routable (ID ≠ 0) | 390 |
| CLI-accessible (BOTH + CLI_ONLY) | 403 |
| **Coverage** | **87.0%** |
| Status | ⚠️ ISSUES FOUND |

## Categories

| Category | Commands |
|----------|----------|
| Debug | 28 |
| CLI | 26 |
| Swarm | 25 |
| Router | 21 |
| ReverseEng | 20 |
| Edit | 19 |
| Theme | 17 |
| Hotpatch | 17 |
| File | 15 |
| LSP | 13 |
| ASM | 12 |
| Hybrid | 12 |
| MultiResp | 12 |
| QW | 12 |
| Terminal | 11 |
| Agent | 11 |
| Backend | 11 |
| View | 10 |
| Voice | 10 |
| Transparency | 9 |
| Plugin | 9 |
| LSPServer | 9 |
| PDB | 9 |
| AIContext | 7 |
| Audit | 7 |
| Autonomy | 6 |
| Monaco | 6 |
| Telemetry | 6 |
| Git | 5 |
| SubAgent | 5 |
| Editor | 5 |
| Help | 4 |
| AIMode | 4 |
| Governor | 4 |
| Safety | 4 |
| Replay | 4 |
| Tools | 4 |
| Modules | 3 |
| Confidence | 2 |
| Gauntlet | 2 |

## ⚠️ IDM_* Defines Missing from COMMAND_TABLE

These Win32 GUI command IDs have no entry in COMMAND_TABLE.
To fix: add an X(...) row in `command_registry.hpp`.

| Define | Value | File | Line |
|--------|-------|------|------|
| `IDM_FILE_AUTOSAVE` | 105 | ide_constants.h | 8 |
| `IDM_FILE_CLOSE_FOLDER` | 106 | ide_constants.h | 9 |
| `IDM_FILE_OPEN_FOLDER` | 108 | ide_constants.h | 11 |
| `IDM_FILE_NEW_WINDOW` | 109 | ide_constants.h | 12 |
| `IDM_FILE_CLOSE_TAB` | 110 | ide_constants.h | 13 |
| `IDM_EDIT_SELECTALL` | 208 | ide_constants.h | 22 |
| `IDM_EDIT_MULTICURSOR_ADD` | 209 | ide_constants.h | 23 |
| `IDM_EDIT_MULTICURSOR_REMOVE` | 210 | ide_constants.h | 24 |
| `IDM_EDIT_GOTO_LINE` | 211 | ide_constants.h | 25 |
| `IDM_VIEW_TOGGLE_SIDEBAR` | 301 | ide_constants.h | 27 |
| `IDM_VIEW_TOGGLE_TERMINAL` | 302 | ide_constants.h | 28 |
| `IDM_VIEW_TOGGLE_OUTPUT` | 303 | ide_constants.h | 29 |
| `IDM_VIEW_TOGGLE_FULLSCREEN` | 304 | ide_constants.h | 30 |
| `IDM_VIEW_ZOOM_IN` | 305 | ide_constants.h | 31 |
| `IDM_VIEW_ZOOM_OUT` | 306 | ide_constants.h | 32 |
| `IDM_VIEW_ZOOM_RESET` | 307 | ide_constants.h | 33 |
| `IDM_AI_INLINE_COMPLETE` | 401 | ide_constants.h | 35 |
| `IDM_AI_CHAT_MODE` | 402 | ide_constants.h | 36 |
| `IDM_AI_EXPLAIN_CODE` | 403 | ide_constants.h | 37 |
| `IDM_AI_REFACTOR` | 404 | ide_constants.h | 38 |
| `IDM_AI_GENERATE_TESTS` | 405 | ide_constants.h | 39 |
| `IDM_AI_GENERATE_DOCS` | 406 | ide_constants.h | 40 |
| `IDM_AI_FIX_ERRORS` | 407 | ide_constants.h | 41 |
| `IDM_AI_OPTIMIZE_CODE` | 408 | ide_constants.h | 42 |
| `IDM_AI_MODEL_SELECT` | 409 | ide_constants.h | 43 |
| `IDM_TOOLS_COMMAND_PALETTE` | 501 | ide_constants.h | 45 |
| `IDM_TOOLS_SETTINGS` | 502 | ide_constants.h | 46 |
| `IDM_TOOLS_EXTENSIONS` | 503 | ide_constants.h | 47 |
| `IDM_TOOLS_TERMINAL` | 504 | ide_constants.h | 48 |
| `IDM_TOOLS_BUILD` | 505 | ide_constants.h | 49 |
| `IDM_TOOLS_DEBUG` | 506 | ide_constants.h | 50 |
| `IDM_HELP_DOCS` | 601 | ide_constants.h | 52 |
| `IDM_HELP_SHORTCUTS` | 603 | ide_constants.h | 54 |
| `IDM_DECOMP_RENAME_VAR` | 8001 | win32app/Win32IDE_DecompilerView.cpp | 54 |
| `IDM_DECOMP_GOTO_DEF` | 8002 | win32app/Win32IDE_DecompilerView.cpp | 55 |
| `IDM_DECOMP_FIND_REFS` | 8003 | win32app/Win32IDE_DecompilerView.cpp | 56 |
| `IDM_DECOMP_COPY_LINE` | 8004 | win32app/Win32IDE_DecompilerView.cpp | 57 |
| `IDM_DECOMP_COPY_ALL` | 8005 | win32app/Win32IDE_DecompilerView.cpp | 58 |
| `IDM_DECOMP_GOTO_ADDR` | 8006 | win32app/Win32IDE_DecompilerView.cpp | 59 |
| `IDM_VSCEXT_API_STATUS` | 10000 | modules/vscode_extension_api.h | 1900 |
| `IDM_VSCEXT_API_RELOAD` | 10001 | modules/vscode_extension_api.h | 1901 |
| `IDM_VSCEXT_API_LIST_COMMANDS` | 10002 | modules/vscode_extension_api.h | 1902 |
| `IDM_VSCEXT_API_LIST_PROVIDERS` | 10003 | modules/vscode_extension_api.h | 1903 |
| `IDM_VSCEXT_API_DIAGNOSTICS` | 10004 | modules/vscode_extension_api.h | 1904 |
| `IDM_VSCEXT_API_EXTENSIONS` | 10005 | modules/vscode_extension_api.h | 1905 |
| `IDM_VSCEXT_API_STATS` | 10006 | modules/vscode_extension_api.h | 1906 |
| `IDM_VSCEXT_API_LOAD_NATIVE` | 10007 | modules/vscode_extension_api.h | 1907 |
| `IDM_VSCEXT_API_DEACTIVATE_ALL` | 10008 | modules/vscode_extension_api.h | 1908 |
| `IDM_VSCEXT_API_EXPORT_CONFIG` | 10009 | modules/vscode_extension_api.h | 1909 |
| `IDM_VOICE_AUTO_TOGGLE` | 10200 | win32app/Win32IDE_VoiceAutomation.cpp | 26 |
| `IDM_VOICE_AUTO_SETTINGS` | 10201 | win32app/Win32IDE_VoiceAutomation.cpp | 27 |
| `IDM_VOICE_AUTO_NEXT_VOICE` | 10202 | win32app/Win32IDE_VoiceAutomation.cpp | 28 |
| `IDM_VOICE_AUTO_PREV_VOICE` | 10203 | win32app/Win32IDE_VoiceAutomation.cpp | 29 |
| `IDM_VOICE_AUTO_RATE_UP` | 10204 | win32app/Win32IDE_VoiceAutomation.cpp | 30 |
| `IDM_VOICE_AUTO_RATE_DOWN` | 10205 | win32app/Win32IDE_VoiceAutomation.cpp | 31 |
| `IDM_VOICE_AUTO_STOP` | 10206 | win32app/Win32IDE_VoiceAutomation.cpp | 32 |

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
- `handleHelpDocs`
- `handleHelpShortcuts`
- `handleHistory`
- `handleMultiResponse`
- `handlePolicy`
- `handleReplay`
- `handleSafety`
- `handleSwarmDistribute`
- `handleSwarmRebalance`
- `handleTerminalNew`
- `handleTools`
